/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// playerbot_movement.cpp: Manages bot movements

#include "playerbot.h"
#include "debuglines.h"
#include "g_phys.h"

static int maxFallHeight = 400;

static constexpr float BOT_YIELD_CONTACT_MARGIN = 8.0f;
static constexpr float BOT_YIELD_DISTANCE       = 48.0f;
static constexpr float BOT_YIELD_STOP_DISTANCE  = 8.0f;
static constexpr float BOT_YIELD_MIN_INPUT      = 16.0f;
static constexpr float BOT_YIELD_MIN_ALIGNMENT  = 0.5f;
static constexpr int   BOT_YIELD_DURATION_MS    = 750;
static constexpr int   BOT_YIELD_COOLDOWN_MS    = 500;

BotMovement::BotMovement()
{
    controlledEntity = NULL;

    m_pPath          = NULL;
    m_iLastMoveTime  = 0;
    m_fAttractTime   = 0;
    m_bPathing       = false;
    m_bMoveCompleted = false;

    m_stuck.reset();
    m_collision.reset();
    m_jump.reset();
    m_ladder.reset();
    m_yield.reset();
    m_bGaveUp             = false;
    m_stuckPolicy         = BotStuckPolicy::TrackAndGiveUp;
    m_pWaitingForLadder   = nullptr;

    for (int i = 0; i < BOT_BANNED_ZONES_MAX; i++) {
        m_bannedZones[i].expireTime = 0;
    }
}

BotMovement::~BotMovement()
{
    delete m_pPath;
}

void BotMovement::SetControlledEntity(Player *newEntity)
{
    controlledEntity = newEntity;
    m_yield.reset();
}

void BotMovement::SetWaitingForLadder(FuncLadder *ladder)
{
    m_pWaitingForLadder = ladder;
}

void BotMovement::MoveThink(usercmd_t& botcmd)
{
    Vector vAngles;
    Vector vWishDir;
    Vector vDelta;

    CheckAttractiveNodes();

    if (CheckLadderRespawnFallback(botcmd)) {
        return;
    }

    if (ApplyPlayerYield(botcmd)) {
        return;
    }

    if (!IsMoving() || !m_pPath) {
        // Changed in OPM
        //  Don't zero rightmove/upmove here — other states (attack strafing,
        //  crouching) may have already set them. Only zero forwardmove since
        //  the movement system owns forward/backward when there's no active path.
        botcmd.forwardmove = 0;
        return;
    }

    if (m_pWaitingForLadder) {
        if (m_pWaitingForLadder->IsClaimedByOther(controlledEntity)) {
            botcmd.forwardmove = 0;
            botcmd.rightmove   = 0;
            botcmd.upmove      = 0;
            m_stuck.reset();
            return;
        }

        m_pWaitingForLadder = nullptr;
    }

    if (m_pPath->GetNodeCount()) {
        m_vTargetPos = m_pPath->GetDestination();
    }

    if (m_pPath->IsQuerying()) {
        m_iLastMoveTime = level.inttime;
    }

    if (level.inttime >= m_iLastMoveTime + 5000 && m_vCurrentOrigin != controlledEntity->origin) {
        m_vCurrentOrigin = controlledEntity->origin;

        if (m_pPath->GetNodeCount() && !controlledEntity->GetLadder()) {
            // recalculate paths because of a new origin

            PathSearchParameter parameters;
            parameters.entity     = controlledEntity;
            parameters.fallHeight = maxFallHeight;
            m_pPath->FindPath(controlledEntity->origin, m_pPath->GetDestination(), parameters);
        }

        m_iLastMoveTime = level.inttime;
    }

    vDelta = m_pPath->GetCurrentDelta();
    vDelta = FixDeltaFromCollision(vDelta);

    if (m_pPath->GetNodeCount()) {
        m_pPath->UpdatePos(controlledEntity->origin);

        m_vCurrentGoal = controlledEntity->origin;
        VectorAdd2D(m_vCurrentGoal, vDelta, m_vCurrentGoal);

        if (MoveDone()) {
            ClearMove(true);
            botcmd.forwardmove = 0;
            return;
        }
    }

    if (ai_debugpath->integer) {
        G_DebugLine(controlledEntity->centroid, m_vCurrentGoal + Vector(0, 0, 36), 1, 1, 0, 1);
    }

    // Combat pursuit still needs blocked-path recovery, but only patrol-style
    // movement should blacklist goals and report a give-up back to the intent layer.
    if (m_stuckPolicy != BotStuckPolicy::Ignore && level.inttime >= m_stuck.checkTime + 1000
        && !controlledEntity->GetLadder() && !m_pWaitingForLadder) {
        m_stuck.checkTime = level.inttime;

        const Vector& cur = controlledEntity->origin;

        m_stuck.positions[m_stuck.nextSlot] = cur;
        m_stuck.nextSlot                    = (m_stuck.nextSlot + 1) % BOT_STUCK_HISTORY;
        if (m_stuck.sampleCount < BOT_STUCK_HISTORY) {
            m_stuck.sampleCount++;
        }

        if (m_stuck.sampleCount == BOT_STUCK_HISTORY) {
            // oldest sample is at nextSlot (already overwritten above, so compare to cur)
            const Vector& oldest = m_stuck.positions[m_stuck.nextSlot];
            const float   dist   = (cur - oldest).lengthSquared();

            if (ai_debugpath->integer) {
                gi.Printf(
                    "BOT[%d] stuck check: dist over %ds = %.0f\n",
                    controlledEntity->entnum,
                    BOT_STUCK_HISTORY,
                    sqrtf(dist)
                );
            }

            if (dist < Square(128)) {
                if (ai_debugpath->integer) {
                    gi.Printf(
                        "BOT[%d] STUCK at (%.0f %.0f %.0f) - recovering\n",
                        controlledEntity->entnum,
                        cur.x,
                        cur.y,
                        cur.z
                    );
                }

                const BotStuckPolicy activePolicy = m_stuckPolicy;
                const bool           avoidStarted = AvoidPath(controlledEntity->origin, 256.0f, vec_zero, activePolicy);

                m_collision.crouchUntil = level.inttime + 1500;
                if (activePolicy == BotStuckPolicy::TrackAndGiveUp) {
                    if (avoidStarted) {
                        BanCurrentZone();
                    }
                    m_bGaveUp = true;
                }
                m_stuck.reset();
            }
        }
    }

    if (ai_debugpath->integer) {
        int i;
        int nodecount = m_pPath->GetNodeCount();

        for (i = 0; i < nodecount - 1; i++) {
            PathNav      node1  = m_pPath->GetNode(i);
            PathNav      node2  = m_pPath->GetNode(i + 1);
            const Vector vStart = node1.origin + Vector(0, 0, 32);
            const Vector vEnd   = node2.origin + Vector(0, 0, 32);

            G_DebugLine(vStart, vEnd, 1, 0, 0, 1);
        }
    }

    if (m_pPath->GetNodeCount()) {
        if ((m_vTargetPos - controlledEntity->origin).lengthSquared() <= Square(16)) {
            ClearMove(true);
            botcmd.forwardmove = 0;
            return;
        }
    } else {
        ClearMove();
        botcmd.forwardmove = 0;
        return;
    }

    // Rotate the dir
    if (m_pPath->GetNodeCount()) {
        m_vCurrentDir = CalculateDir(vDelta);
    } else {
        m_vCurrentDir = CalculateDir(m_vCurrentGoal - controlledEntity->origin);
    }

    vWishDir = CalculateRelativeWishDirection(m_vCurrentDir);

    // Forward to the specified direction
    float x = vWishDir.x * 127;
    float y = -vWishDir.y * 127;

    botcmd.forwardmove = (signed char)Q_clamp(x, -127, 127);
    botcmd.rightmove   = (signed char)Q_clamp(y, -127, 127);
    botcmd.upmove      = 0;

    CheckJump(botcmd);

    if (!m_jump.active) {
        CheckJumpOverEdge(botcmd);
    }

    if (!botcmd.upmove && level.inttime < m_collision.crouchUntil) {
        botcmd.upmove = -127;
    }
}

bool BotMovement::ApplyPlayerYield(usercmd_t& botcmd)
{
    if (!controlledEntity || controlledEntity->IsDead() || controlledEntity->IsSpectator()
        || controlledEntity->GetLadder() || (controlledEntity->flags & (FL_IMMOBILE | FL_PARTIAL_IMMOBILE))
        || (!controlledEntity->groundentity && !controlledEntity->client->ps.walking)) {
        m_yield.active = false;
        return false;
    }

    if (m_yield.active) {
        const Vector remaining = m_yield.destination - controlledEntity->origin;
        if (level.inttime >= m_yield.expireTime || remaining.lengthXYSquared() <= Square(BOT_YIELD_STOP_DISTANCE)) {
            m_yield.active        = false;
            m_yield.cooldownUntil = level.inttime + BOT_YIELD_COOLDOWN_MS;
        }
    }

    if (!m_yield.active && level.inttime >= m_yield.cooldownUntil) {
        Vector pushDirection;
        Vector destination;

        if (FindPlayerPushDirection(pushDirection) && FindYieldDestination(pushDirection, destination)) {
            m_yield.active      = true;
            m_yield.expireTime  = level.inttime + BOT_YIELD_DURATION_MS;
            m_yield.direction   = pushDirection;
            m_yield.destination = destination;
        }
    }

    if (!m_yield.active) {
        return false;
    }

    const Vector wishDir = CalculateRelativeWishDirection(m_yield.direction);
    float        forward = wishDir.x * 127.0f;
    float        right   = -wishDir.y * 127.0f;
    botcmd.forwardmove   = (signed char)Q_clamp(forward, -127.0f, 127.0f);
    botcmd.rightmove     = (signed char)Q_clamp(right, -127.0f, 127.0f);
    return true;
}

bool BotMovement::FindPlayerPushDirection(Vector& pushDirection) const
{
    const Vector botMins  = controlledEntity->origin + controlledEntity->mins;
    const Vector botMaxs  = controlledEntity->origin + controlledEntity->maxs;
    float        bestScore = BOT_YIELD_MIN_ALIGNMENT;
    bool         found     = false;

    for (int i = 0; i < game.maxclients; i++) {
        gentity_t *edict = &g_entities[i];
        if (!edict->inuse || !edict->entity || edict->entity == controlledEntity
            || !edict->entity->isSubclassOf(Player)) {
            continue;
        }

        Player *player = static_cast<Player *>(edict->entity);
        if (player->IsDead() || player->IsSpectator() || player->getMoveType() == MOVETYPE_NOCLIP) {
            continue;
        }

        const Vector playerMins = player->origin + player->mins;
        const Vector playerMaxs = player->origin + player->maxs;
        if (playerMaxs.x + BOT_YIELD_CONTACT_MARGIN < botMins.x
            || playerMins.x - BOT_YIELD_CONTACT_MARGIN > botMaxs.x
            || playerMaxs.y + BOT_YIELD_CONTACT_MARGIN < botMins.y
            || playerMins.y - BOT_YIELD_CONTACT_MARGIN > botMaxs.y || playerMaxs.z < botMins.z
            || playerMins.z > botMaxs.z) {
            continue;
        }

        const usercmd_t& playerCmd = player->GetLastUsercmd();
        Vector           forward;
        Vector           left;
        Vector(0, player->GetViewAngles().y, 0).AngleVectorsLeft(&forward, &left, NULL);

        Vector inputDirection = forward * playerCmd.forwardmove - left * playerCmd.rightmove;
        inputDirection.z      = 0;
        if (inputDirection.lengthXYSquared() < Square(BOT_YIELD_MIN_INPUT)) {
            continue;
        }
        VectorNormalize2D(inputDirection);

        Vector towardBot = controlledEntity->origin - player->origin;
        towardBot.z      = 0;
        if (towardBot.lengthXYSquared() < 1.0f) {
            continue;
        }
        VectorNormalize2D(towardBot);

        const float alignment = DotProduct2D(inputDirection, towardBot);
        if (alignment > bestScore) {
            bestScore     = alignment;
            pushDirection = inputDirection;
            found         = true;
        }
    }

    return found;
}

bool BotMovement::FindYieldDestination(const Vector& pushDirection, Vector& destination) const
{
    const int    traceMask = (MASK_PLAYERSOLID | CONTENTS_BOTCLIP) & ~CONTENTS_BODY;
    const Vector start     = controlledEntity->origin;
    const Vector end       = start + pushDirection * BOT_YIELD_DISTANCE;
    const trace_t moveTrace = G_Trace(
        start,
        controlledEntity->mins,
        controlledEntity->maxs,
        end,
        controlledEntity,
        traceMask,
        qtrue,
        "BotMovement::FindYieldDestination"
    );
    const Vector moveEnd = moveTrace.endpos;

    if (moveTrace.startsolid || (moveEnd - start).lengthXYSquared() < Square(BOT_YIELD_STOP_DISTANCE)) {
        return false;
    }

    const Vector midpoint     = start + (moveEnd - start) * 0.5f;
    const Vector groundOffset = Vector(0, 0, STEPSIZE * 2.0f);
    const trace_t middleGround = G_Trace(
        midpoint,
        controlledEntity->mins,
        controlledEntity->maxs,
        midpoint - groundOffset,
        controlledEntity,
        traceMask,
        qtrue,
        "BotMovement::FindYieldDestination"
    );
    const trace_t endGround = G_Trace(
        moveEnd,
        controlledEntity->mins,
        controlledEntity->maxs,
        moveEnd - groundOffset,
        controlledEntity,
        traceMask,
        qtrue,
        "BotMovement::FindYieldDestination"
    );

    if (middleGround.fraction == 1.0f || middleGround.plane.normal[2] < MIN_WALK_NORMAL
        || endGround.fraction == 1.0f || endGround.plane.normal[2] < MIN_WALK_NORMAL) {
        return false;
    }

    destination = moveEnd;
    return true;
}

Vector BotMovement::CalculateDir(const Vector& delta) const
{
    Vector dir;

    dir    = delta;
    dir[2] = 0;
    VectorNormalize2D(dir);

    return dir;
}

Vector BotMovement::CalculateRelativeWishDirection(const Vector& dir) const
{
    Vector angles;
    Vector wishdir;

    angles = dir.toAngles() - controlledEntity->angles;
    angles.AngleVectorsLeft(&wishdir);

    return wishdir;
}

void BotMovement::CheckAttractiveNodes()
{
    for (int i = m_attractList.NumObjects(); i > 0; i--) {
        nodeAttract_t *a = m_attractList.ObjectAt(i);

        if (a->m_pNode == NULL || !a->m_pNode->CheckTeam(controlledEntity) || level.time > a->m_fRespawnTime) {
            delete a;
            m_attractList.RemoveObjectAt(i);
        }
    }
}

void BotMovement::CheckEndPos(Entity *entity)
{
    Vector  start;
    Vector  end;
    trace_t trace;

    if (!m_pPath->GetNodeCount()) {
        return;
    }

    start = m_pPath->GetDestination();
    end   = m_vTargetPos;

    trace =
        G_Trace(start, entity->mins, entity->maxs, end, entity, MASK_TARGETPATH, true, "BotController::CheckEndPos");

    if (trace.fraction < 0.95f) {
        m_vTargetPos = trace.endpos;
    }
}

void BotMovement::CheckJump(usercmd_t& botcmd)
{
    Vector  start;
    Vector  end;
    Vector  dir;
    Vector  delta;
    trace_t trace;

    if (controlledEntity->GetLadder()) {
        if (g_navigation_legacy->integer) {
            botcmd.upmove = botcmd.upmove ? 0 : 127;
        } else if (!m_pPath->GetNodeCount()) {
            // If the bot is not moving, cancel it
            botcmd.upmove = botcmd.upmove ? 0 : 127;
        }
        return;
    }

    if (!controlledEntity->groundentity && !controlledEntity->client->ps.walking) {
        // Falling
        m_jump.active = false;
        return;
    }

    dir = m_vCurrentDir;

    start = controlledEntity->origin + Vector(0, 0, STEPSIZE);
    end =
        controlledEntity->origin + Vector(0, 0, STEPSIZE) + dir * (controlledEntity->maxs.y - controlledEntity->mins.y);

    if (ai_debugpath->integer) {
        G_DebugLine(start, end, 1, 0, 1, 1);
    }

    // Check if the bot needs to jump
    trace = G_Trace(
        start,
        controlledEntity->mins,
        controlledEntity->maxs,
        end,
        controlledEntity,
        MASK_PLAYERSOLID,
        false,
        "BotController::CheckJump"
    );

    // No need to jump
    if (!trace.startsolid && trace.fraction > 0.5f) {
        m_jump.active = false;
        return;
    }

    start = controlledEntity->origin;
    end   = controlledEntity->origin;
    end.z += STEPSIZE * 3;
    end.z += STEPSIZE / 1.5;

    if (ai_debugpath->integer) {
        G_DebugLine(start, end, 1, 0, 1, 1);
    }

    // Check if the bot can jump up
    trace = G_Trace(
        start,
        controlledEntity->mins,
        controlledEntity->maxs,
        end,
        controlledEntity,
        MASK_PLAYERSOLID,
        true,
        "BotController::CheckJump"
    );

    start = trace.endpos;
    end   = trace.endpos + dir * (controlledEntity->maxs.y - controlledEntity->mins.y);

    if (ai_debugpath->integer) {
        G_DebugLine(start, end, 1, 0, 1, 1);
    }

    Vector bounds[2];
    bounds[0] = Vector(controlledEntity->mins[0], controlledEntity->mins[1], 0);
    bounds[1] = Vector(
        controlledEntity->maxs[0],
        controlledEntity->maxs[1],
        (controlledEntity->maxs[0] + controlledEntity->maxs[1]) * 0.5
    );

    // Check if the bot can jump at the location
    trace = G_Trace(
        start, bounds[0], bounds[1], end, controlledEntity, MASK_PLAYERSOLID, false, "BotController::CheckJump"
    );

    if (trace.plane.normal[2] <= MIN_WALK_NORMAL && trace.fraction < 1) {
        m_jump.active = false;
        return;
    }

    if (!m_jump.active) {
        m_jump.active    = true;
        m_jump.checkTime = level.inttime;
        m_jump.startPos  = controlledEntity->origin;
    } else if (level.inttime > m_jump.checkTime + 100) {
        m_jump.active = false;

        delta = m_jump.startPos - controlledEntity->origin;
        if (delta.lengthSquared() < Square(32)) {
            botcmd.upmove = 127;
        }
    }
}

void BotMovement::CheckJumpOverEdge(usercmd_t& botcmd)
{
    Vector  start;
    Vector  end;
    Vector  dir;
    trace_t trace;

    if (!controlledEntity->groundentity && !controlledEntity->client->ps.walking) {
        // Falling
        return;
    }

    dir = m_vCurrentDir;

    start = controlledEntity->origin + Vector(0, 0, STEPSIZE);
    end =
        controlledEntity->origin + Vector(0, 0, STEPSIZE) + dir * (controlledEntity->maxs.y - controlledEntity->mins.y);

    if (ai_debugpath->integer) {
        G_DebugLine(start, end, 1, 0, 1, 1);
    }

    // Check if the bot needs to jump
    trace = G_Trace(
        start,
        controlledEntity->mins,
        controlledEntity->maxs,
        end,
        controlledEntity,
        MASK_PLAYERSOLID,
        false,
        "BotController::CheckJumpOverEdge"
    );

    if (trace.fraction < 1) {
        // Blocked
        return;
    }

    //
    // Check if falling
    //

    start = trace.endpos;
    end   = start - Vector(0, 0, STEPSIZE * 2);

    trace = G_Trace(
        start,
        controlledEntity->mins,
        controlledEntity->maxs,
        end,
        controlledEntity,
        MASK_PLAYERSOLID,
        false,
        "BotController::CheckJumpOverEdge"
    );

    if (trace.fraction != 1.0) {
        // Blocked
        return;
    }

    //
    // Check if there is an edge at the end
    //

    end = start + dir * controlledEntity->GetRunSpeed() / 2.0;
    end -= Vector(0, 0, STEPSIZE * 2);

    trace = G_Trace(
        start,
        controlledEntity->mins,
        controlledEntity->maxs,
        end,
        controlledEntity,
        MASK_PLAYERSOLID,
        false,
        "BotController::CheckJumpOverEdge"
    );

    if (trace.fraction == 1) {
        return;
    }

    if (!botcmd.upmove) {
        botcmd.upmove = 127;
    } else {
        botcmd.upmove = 0;
    }
}

bool BotMovement::CheckLadderRespawnFallback(usercmd_t& botcmd)
{
    if (!controlledEntity || !controlledEntity->GetLadder() || g_gametype->integer == GT_SINGLE_PLAYER) {
        m_ladder.reset();
        return false;
    }

    if (!m_ladder.active) {
        m_ladder.active           = true;
        m_ladder.lastProgressTime = level.inttime;
        m_ladder.lastProgressPos  = controlledEntity->origin;
        m_ladder.respawnQueued    = false;
        return false;
    }

    if ((controlledEntity->origin - m_ladder.lastProgressPos).lengthSquared() >= Square(BOT_LADDER_PROGRESS_DIST)) {
        m_ladder.lastProgressTime = level.inttime;
        m_ladder.lastProgressPos  = controlledEntity->origin;
        return false;
    }

    if (m_ladder.respawnQueued || level.inttime < m_ladder.lastProgressTime + BOT_LADDER_RESPAWN_TIME_MS) {
        return false;
    }

    if (ai_debugpath->integer) {
        gi.Printf(
            "BOT[%d] ladder stuck for %ds - respawning\n",
            controlledEntity->entnum,
            BOT_LADDER_RESPAWN_TIME_MS / 1000
        );
    }

    m_ladder.respawnQueued = true;
    controlledEntity->PostEvent(EV_Player_Respawn, 0);
    ClearMove();
    botcmd.forwardmove = 0;
    botcmd.rightmove   = 0;
    botcmd.upmove      = 0;
    return true;
}

/*
====================
AvoidPath

Avoid the specified position within the radius and start from a direction
====================
*/
bool BotMovement::AvoidPath(
    Vector         vAvoid,
    float          fAvoidRadius,
    Vector         vPreferredDir,
    BotStuckPolicy stuckPolicy,
    float         *vLeashHome,
    float          fLeashRadius
)
{
    Vector vDir;

    if (vPreferredDir == vec_zero) {
        vDir = controlledEntity->origin - vAvoid;
        VectorNormalizeFast(vDir);
    } else {
        vDir = vPreferredDir;
    }

    PathSearchParameter parameters;
    parameters.entity     = controlledEntity;
    parameters.fallHeight = maxFallHeight;
    parameters.leashDist  = fLeashRadius;
    if (vLeashHome) {
        parameters.leashHome = vLeashHome;
    }

    if (!m_pPath) {
        m_pPath = IPather::CreatePather();
    }

    m_stuckPolicy = stuckPolicy;
    m_pPath->FindPathAway(controlledEntity->origin, vAvoid, vDir, fAvoidRadius, parameters);

    NewMove();

    if (!m_pPath->GetNodeCount()) {
        // No escape node — leave pathing inactive so the caller can fall back.
        m_bPathing = false;
        return false;
    }

    m_iLastMoveTime = level.inttime;
    m_vTargetPos    = m_pPath->GetDestination();
    return true;
}

/*
====================
MoveNear

Move near the specified position within the radius
====================
*/
bool BotMovement::MoveNear(
    Vector vNear, float fRadius, BotStuckPolicy stuckPolicy, float *vLeashHome, float fLeashRadius
)
{
    if (stuckPolicy == BotStuckPolicy::TrackAndGiveUp && IsPositionBanned(vNear)) {
        if (ai_debugpath->integer) {
            gi.Printf(
                "BOT[%d] MoveNear BANNED (%.0f %.0f %.0f)\n", controlledEntity->entnum, vNear.x, vNear.y, vNear.z
            );
        }
        m_bGaveUp = true;
        ClearMove();
        return false;
    }

    PathSearchParameter parameters;
    parameters.entity     = controlledEntity;
    parameters.fallHeight = maxFallHeight;
    parameters.leashDist  = fLeashRadius;
    if (vLeashHome) {
        parameters.leashHome = vLeashHome;
    }

    if (!m_pPath) {
        m_pPath = IPather::CreatePather();
    }

    m_stuckPolicy = stuckPolicy;
    m_pPath->FindPathNear(controlledEntity->origin, vNear, fRadius, parameters);
    NewMove();

    if (!m_pPath->GetNodeCount()) {
        m_bPathing = false;
        return false;
    }

    if (stuckPolicy == BotStuckPolicy::TrackAndGiveUp && PathTouchesBannedZone()) {
        if (ai_debugpath->integer) {
            gi.Printf(
                "BOT[%d] MoveNear PATH BANNED (%.0f %.0f %.0f)\n", controlledEntity->entnum, vNear.x, vNear.y, vNear.z
            );
        }
        m_bGaveUp = true;
        ClearMove();
        return false;
    }

    m_iLastMoveTime = level.inttime;
    m_vTargetPos    = m_pPath->GetDestination();
    return true;
}

/*
====================
MoveTo

Move to the specified position
====================
*/
bool BotMovement::MoveTo(Vector vPos, BotStuckPolicy stuckPolicy, float *vLeashHome, float fLeashRadius)
{
    if (stuckPolicy == BotStuckPolicy::TrackAndGiveUp && IsPositionBanned(vPos)) {
        if (ai_debugpath->integer) {
            gi.Printf("BOT[%d] MoveTo BANNED (%.0f %.0f %.0f)\n", controlledEntity->entnum, vPos.x, vPos.y, vPos.z);
        }
        m_bGaveUp = true;
        ClearMove();
        return false;
    }

    m_vTargetPos = vPos;

    PathSearchParameter parameters;
    parameters.entity     = controlledEntity;
    parameters.fallHeight = maxFallHeight;
    parameters.leashDist  = fLeashRadius;
    if (vLeashHome) {
        parameters.leashHome = vLeashHome;
    }

    if (!m_pPath) {
        m_pPath = IPather::CreatePather();
    }

    m_stuckPolicy = stuckPolicy;
    m_pPath->FindPath(controlledEntity->origin, vPos, parameters);

    NewMove();

    if (!m_pPath->GetNodeCount()) {
        m_bPathing = false;
        return false;
    }

    if (stuckPolicy == BotStuckPolicy::TrackAndGiveUp && PathTouchesBannedZone()) {
        if (ai_debugpath->integer) {
            gi.Printf(
                "BOT[%d] MoveTo PATH BANNED (%.0f %.0f %.0f)\n", controlledEntity->entnum, vPos.x, vPos.y, vPos.z
            );
        }
        m_bGaveUp = true;
        ClearMove();
        return false;
    }

    m_iLastMoveTime = level.inttime;
    CheckEndPos(controlledEntity);
    return true;
}

/*
====================
MoveToBestAttractivePoint

Move to the nearest attractive point with a minimum priority
Returns true if no attractive point was found
====================
*/
bool BotMovement::MoveToBestAttractivePoint(int iMinPriority)
{
    Container<AttractiveNode *> list;
    AttractiveNode             *bestNode;
    float                       bestDistanceSquared;
    int                         bestPriority;

    if (m_pPrimaryAttract) {
        MoveTo(m_pPrimaryAttract->origin);

        if (!IsMoving()) {
            m_pPrimaryAttract = NULL;
        } else {
            if (MoveDone()) {
                if (!m_fAttractTime) {
                    m_fAttractTime = level.time + m_pPrimaryAttract->m_fMaxStayTime;
                }
                if (level.time > m_fAttractTime) {
                    nodeAttract_t *a  = new nodeAttract_t;
                    a->m_fRespawnTime = level.time + m_pPrimaryAttract->m_fRespawnTime;
                    a->m_pNode        = m_pPrimaryAttract;

                    m_pPrimaryAttract = NULL;
                }
            }

            return true;
        }
    }

    if (!attractiveNodes.NumObjects()) {
        return false;
    }

    bestNode            = NULL;
    bestDistanceSquared = 99999999.0f;
    bestPriority        = iMinPriority;

    for (int i = attractiveNodes.NumObjects(); i > 0; i--) {
        AttractiveNode *node = attractiveNodes.ObjectAt(i);
        float           distSquared;
        bool            m_bRespawning = false;

        for (int j = m_attractList.NumObjects(); j > 0; j--) {
            AttractiveNode *node2 = m_attractList.ObjectAt(j)->m_pNode;

            if (node2 == node) {
                m_bRespawning = true;
                break;
            }
        }

        if (m_bRespawning) {
            continue;
        }

        if (node->m_iPriority < bestPriority) {
            continue;
        }

        if (!node->CheckTeam(controlledEntity)) {
            continue;
        }

        distSquared = VectorLengthSquared(controlledEntity->origin - node->origin);

        if (node->m_fMaxDistanceSquared >= 0 && distSquared > node->m_fMaxDistanceSquared) {
            continue;
        }

        if (!CanMoveTo(node->origin)) {
            continue;
        }

        if (distSquared < bestDistanceSquared) {
            bestDistanceSquared = distSquared;
            bestNode            = node;
            bestPriority        = node->m_iPriority;
        }
    }

    if (bestNode) {
        m_pPrimaryAttract = bestNode;
        m_fAttractTime    = 0;
        MoveTo(bestNode->origin);
        return true;
    } else {
        // No attractive point found
        return false;
    }
}

/*
====================
NewMove

Called when there is a new move
====================
*/
void BotMovement::NewMove()
{
    m_bPathing       = true;
    m_bGaveUp        = false;
    m_bMoveCompleted = false;

    if (m_stuckPolicy != BotStuckPolicy::Ignore) {
        m_stuck.reset();
    }
}

void BotMovement::CalculateBestFrontAvoidance(
    const Vector& targetOrg, float maxDist, const Vector& forward, const Vector& right, float& bestFrac, Vector& bestPos
)
{
    Vector  mins, maxs;
    bool    wasOnGround = true;
    Vector  start, step;
    Vector  entityStepOrg;
    trace_t trace;
    int     i;

    bestFrac = 0;
    bestPos  = vec_zero;

    mins = controlledEntity->mins;
    maxs = controlledEntity->maxs;
    maxs.z -= STEPSIZE;
    entityStepOrg = controlledEntity->origin + Vector(0, 0, STEPSIZE);

    for (i = 1; i < 5; i++) {
        start = entityStepOrg - forward + right * (32 * i);
        if (i == 1) {
            step = start;
        }

        //
        // Trace to the right
        //
        trace = G_Trace(entityStepOrg, mins, maxs, start, controlledEntity, MASK_PLAYERSOLID, qtrue, "GetCurrentDelta");

        if (trace.startsolid || trace.fraction <= 0) {
            break;
        }

        start   = trace.endpos;
        start.z = step.z;

        // Make sure the bot can jump after falling
        trace = G_Trace(
            start,
            mins,
            maxs,
            start - Vector(0, 0, STEPSIZE + STEPSIZE * 3),
            controlledEntity,
            MASK_PLAYERSOLID,
            qtrue,
            "GetCurrentDelta"
        );
        if (trace.fraction == 1) {
            if (!wasOnGround) {
                break;
            }

            wasOnGround = false;
            continue;
        }

        wasOnGround = true;
        step        = trace.endpos;

        //
        // Trace from the right to the node
        //
        trace = G_Trace(start, mins, maxs, targetOrg, controlledEntity, MASK_PLAYERSOLID, qtrue, "GetCurrentDelta");
        if (trace.fraction == 0) {
            trace = G_Trace(
                start,
                mins,
                maxs,
                start + forward * Q_min(maxDist, 64),
                controlledEntity,
                MASK_PLAYERSOLID,
                qtrue,
                "GetCurrentDelta"
            );
            trace = G_Trace(
                trace.endpos, mins, maxs, targetOrg, controlledEntity, MASK_PLAYERSOLID, qtrue, "GetCurrentDelta"
            );
        }

        if (trace.fraction > bestFrac) {
            bestFrac = trace.fraction;
            bestPos  = start;
        }
        if (trace.fraction >= 0.999) {
            break;
        }
    }
}

Vector BotMovement::FixDeltaFromCollision(const Vector& delta)
{
    trace_t trace;
    Vector  stepOrg;
    Vector  mins;
    Vector  maxs;
    Vector  newDelta;
    Vector  angles;
    Vector  forward, right, up;
    Vector  target;
    Vector  targetStepOrg;
    Vector  dest;
    Vector  front;
    float   dist;
    float   maxDist;

    if (controlledEntity->GetLadder()) {
        return delta;
    }

    if (level.inttime < m_collision.checkTime + 250 || m_jump.active) {
        if (m_collision.active) {
            newDelta = m_collision.avoidancePos - controlledEntity->origin;
            if (newDelta.lengthSquared() > Square(16)) {
                // Not reached
                return newDelta;
            }

            // Path has been reached so clear the collision
            m_collision.active = false;
        }

        return delta;
    }

    m_collision.checkTime = level.inttime;
    m_collision.active    = false;

    dest     = controlledEntity->origin + delta;
    newDelta = delta;
    dist     = VectorNormalize2(newDelta, forward);
    VectorToAngles(forward, angles);
    AngleVectors(angles, forward, right, up);

    mins = controlledEntity->mins;
    maxs = controlledEntity->maxs;
    maxs.z -= STEPSIZE;

    maxDist = Q_min(dist, 32);

    stepOrg       = controlledEntity->origin + Vector(0, 0, STEPSIZE);
    target        = controlledEntity->origin + forward * maxDist;
    targetStepOrg = target + Vector(0, 0, STEPSIZE);

    trace = G_Trace(stepOrg, mins, maxs, targetStepOrg, controlledEntity, MASK_PLAYERSOLID, qtrue, "GetCurrentDelta");
    if (trace.fraction < 1.0) {
        //
        // Try to use a flat plane instead
        //

        trace_t tmpTrace;
        Vector  forwardXY, rightXY, upXY;
        Vector  targetXY, targetStepOrgXY;

        angles.x = 0;
        AngleVectors(angles, forwardXY, rightXY, upXY);
        targetXY        = controlledEntity->origin + forwardXY * maxDist + Vector(0, 0, STEPSIZE);
        targetStepOrgXY = targetXY + Vector(0, 0, STEPSIZE);

        tmpTrace =
            G_Trace(stepOrg, mins, maxs, targetStepOrgXY, controlledEntity, MASK_PLAYERSOLID, qtrue, "GetCurrentDelta");

        if (tmpTrace.fraction > trace.fraction) {
            trace   = tmpTrace;
            forward = forwardXY;
            right   = rightXY;
            up      = upXY;
            target  = targetXY;
        }
    }

    if (trace.fraction < 1.0) {
        Vector start, step;
        float  bestLeftFrac = 0, bestRightFrac = 0;
        Vector bestLeftPos, bestRightPos;

        // 0 = parallel
        // -1 = perpendicular
        // If it's near parallel use the trace normal
        if (DotProduct(trace.plane.normal, forward) < -0.75) {
            VectorCopy(trace.plane.normal, forward);
            VectorNegate(forward, forward);
            VectorToAngles(forward, angles);
            AngleVectors(angles, forward, right, up);
        }

        //
        // Try to resolve following situation (schema from top):
        //
        // ┌───┐
        // ↑   │
        // p▌  t   ← Must be able to avoid the obstacle in front and move left or right, to target
        // ↓   ↑
        // └─→─┘
        //
        CalculateBestFrontAvoidance(target, 64, forward, right, bestRightFrac, bestRightPos);

        if (bestRightFrac != 1) {
            CalculateBestFrontAvoidance(target, 64, forward, -right, bestLeftFrac, bestLeftPos);
        }

        if (bestLeftFrac != 0 || bestRightFrac != 0) {
            m_collision.active = true;

            //
            // By default use the one with higher fraction
            //
            if (bestLeftFrac > bestRightFrac) {
                m_collision.avoidancePos = bestLeftPos + forward * 64;
            } else if (bestLeftFrac < bestRightFrac) {
                m_collision.avoidancePos = bestRightPos + forward * 64;
            } else {
                // Randomly choose direction if both are the same
                if (Vector::DistanceSquared(bestLeftPos, dest) > Vector::DistanceSquared(bestRightPos, dest)) {
                    m_collision.avoidancePos = bestRightPos + forward * 64;
                } else {
                    m_collision.avoidancePos = bestLeftPos + forward * 64;
                }
            }

            //
            // If falling, make sure to use the one that won't fall
            //
#if 0
            if (leftFallTrace.fraction != rightFallTrace.fraction
                && (leftFallTrace.fraction != 1 || rightFallTrace.fraction != 1)) {
                if (leftFallTrace.fraction == 1 && bestRightFrac) {
                    m_collision.avoidancePos = bestRightPos + forward * 64;
                } else if (rightFallTrace.fraction == 1 && bestLeftFrac) {
                    m_collision.avoidancePos = bestLeftPos + forward * 64;
                }
            }
#endif

            return m_collision.avoidancePos - controlledEntity->origin;
        }
    }

    return delta;
}

/*
====================
CanMoveTo

Returns true if the bot has done moving
====================
*/
bool BotMovement::CanMoveTo(Vector vPos)
{
    if (!controlledEntity) {
        return false;
    }

    if (!m_pPath) {
        m_pPath = IPather::CreatePather();
        if (!m_pPath) {
            return false;
        }
    }

    PathSearchParameter parameters;
    parameters.fallHeight = maxFallHeight;
    parameters.entity     = controlledEntity;
    return m_pPath->TestPath(controlledEntity->origin, vPos, parameters);
}

/*
====================
MoveDone

Returns true if the bot has done moving
====================
*/
bool BotMovement::MoveDone() const
{
    if (!m_bPathing) {
        return true;
    }

    if (!m_pPath->GetNodeCount()) {
        return true;
    }

    Vector delta = m_pPath->GetDestination() - controlledEntity->origin;
    if (delta.lengthXYSquared() < Square(16) && (m_pPath->GetNodeCount() == 1 || delta.z < controlledEntity->maxs.z)) {
        return true;
    }

    return false;
}

bool BotMovement::ReachedMoveGoal() const
{
    return m_bPathing && m_pPath && m_pPath->GetNodeCount() && MoveDone();
}

bool BotMovement::CompletedMove() const
{
    return m_bMoveCompleted;
}

/*
====================
IsMoving

Returns true if the bot has a current path
====================
*/
bool BotMovement::IsMoving() const
{
    return m_bPathing;
}

bool BotMovement::WasGivenUp() const
{
    return m_bGaveUp;
}

bool BotMovement::IsPathSegmentBanned(const Vector& start, const Vector& end) const
{
    Vector      segment      = end - start;
    const float segmentLenSq = segment.lengthSquared();

    for (int i = 0; i < BOT_BANNED_ZONES_MAX; i++) {
        if (m_bannedZones[i].expireTime == 0 || m_bannedZones[i].expireTime <= level.inttime) {
            continue;
        }

        Vector toZone = m_bannedZones[i].origin - start;

        float frac = 0.0f;
        if (segmentLenSq > 0.0f) {
            frac = Q_clamp_float((toZone * segment) / segmentLenSq, 0.0f, 1.0f);
        }

        Vector closest = start + segment * frac;

        if (Vector::DistanceSquared(closest, m_bannedZones[i].origin) <= Square(BOT_BANNED_ZONE_RADIUS)) {
            return true;
        }
    }

    return false;
}

bool BotMovement::PathTouchesBannedZone() const
{
    if (!m_pPath || !m_pPath->GetNodeCount()) {
        return false;
    }

    Vector prev = controlledEntity->origin;

    for (int i = 0; i < m_pPath->GetNodeCount(); i++) {
        const Vector next = m_pPath->GetNode(i).origin;
        if (IsPathSegmentBanned(prev, next)) {
            return true;
        }
        prev = next;
    }

    return IsPathSegmentBanned(prev, m_pPath->GetDestination());
}

bool BotMovement::IsPositionBanned(const Vector& pos) const
{
    for (int i = 0; i < BOT_BANNED_ZONES_MAX; i++) {
        if (m_bannedZones[i].expireTime == 0) {
            continue;
        }
        if (m_bannedZones[i].expireTime <= level.inttime) {
            continue;
        }
        if (Vector::DistanceSquared(m_bannedZones[i].origin, pos) <= Square(BOT_BANNED_ZONE_RADIUS)) {
            return true;
        }
    }
    return false;
}

void BotMovement::ClearBannedZones()
{
    for (int i = 0; i < BOT_BANNED_ZONES_MAX; i++) {
        m_bannedZones[i].expireTime = 0;
    }
}

void BotMovement::BanCurrentZone()
{
    int oldestIdx  = 0;
    int oldestTime = m_bannedZones[0].expireTime;

    for (int i = 1; i < BOT_BANNED_ZONES_MAX; i++) {
        if (m_bannedZones[i].expireTime == 0) {
            oldestIdx = i;
            break;
        }
        if (m_bannedZones[i].expireTime < oldestTime) {
            oldestTime = m_bannedZones[i].expireTime;
            oldestIdx  = i;
        }
    }

    m_bannedZones[oldestIdx].origin     = controlledEntity->origin;
    m_bannedZones[oldestIdx].expireTime = level.inttime + BOT_BANNED_ZONE_DURATION_MS;
}

/*
====================
ClearMove

Stop the bot from moving
====================
*/
void BotMovement::ClearMove(bool completed)
{
    m_bPathing       = false;
    m_bMoveCompleted = completed;
    m_stuck.reset();

    if (m_pPath) {
        m_pPath->Clear();
    }
}

/*
====================
GetCurrentGoal

Return the current goal, usually the nearest node the player should look at
====================
*/
Vector BotMovement::GetCurrentGoal() const
{
    if (!controlledEntity || !m_pPath) {
        return m_vCurrentGoal;
    }

    if (!m_pPath->GetNodeCount()) {
        return m_vCurrentGoal;
    }

    if (!m_pPath->HasReachedGoal(controlledEntity->origin) && m_pPath->GetNodeCount()) {
        const Vector delta = m_pPath->GetCurrentDelta();
        return controlledEntity->origin + Vector(delta[0], delta[1], 0);
    }

    return controlledEntity->origin;
}

Vector BotMovement::GetCurrentPathDirection() const
{
    if (!m_pPath) {
        return vec_zero;
    }

    return m_pPath->GetCurrentDirection();
}
