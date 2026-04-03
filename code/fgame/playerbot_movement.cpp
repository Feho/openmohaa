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
#include "playerbot_beliefs.h"
#include "debuglines.h"
#include "gamecvars.h"

static int maxFallHeight = 400;

BotMovement::BotMovement()
{
    controlledEntity = NULL;

    m_pPath          = NULL;
    m_iLastMoveTime  = 0;
    m_iCheckPathTime = 0;
    m_fAttractTime   = 0;
    m_bPathing       = false;

    m_blocked.reset();
    m_collision.reset();
    m_jump.reset();
    m_progress.reset();

    // Added in OPM
    //  Path blocking state
    m_bGaveUpPath  = false;
    m_vBlockedDest = vec_zero;
}

BotMovement::~BotMovement()
{
    delete m_pPath;
}

void BotMovement::SetControlledEntity(Player *newEntity)
{
    controlledEntity = newEntity;
}

void BotMovement::MoveThink(usercmd_t& botcmd)
{
    Vector vAngles;
    Vector vWishDir;
    Vector vDelta;

    // Added in OPM
    //  Clear give-up flag at start of each frame
    m_bGaveUpPath = false;

    CheckAttractiveNodes();

    if (!IsMoving() || !m_pPath) {
        // Changed in OPM
        //  Don't zero rightmove/upmove here — other states (attack strafing,
        //  crouching) may have already set them. Only zero forwardmove since
        //  the movement system owns forward/backward when there's no active path.
        botcmd.forwardmove = 0;
        return;
    }

    if (m_pPath->GetNodeCount()) {
        m_vTargetPos = m_pPath->GetDestination();
    }

    // Added in OPM
    //  Progress tracking: detect if bot is oscillating without getting closer
    {
        // Check if destination changed - reset progress tracking
        float targetDelta = (m_vTargetPos - m_progress.targetPos).lengthSquared();
        if (targetDelta > Square(64)) {
            // New destination, reset tracking
            float initialDist       = (controlledEntity->origin - m_vTargetPos).length();
            m_progress.targetPos    = m_vTargetPos;
            m_progress.startTime    = level.inttime;
            m_progress.bestDist     = initialDist;
            m_progress.lastProgress = level.inttime;

            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Progress tracking - new target at (%.0f, %.0f, %.0f), dist=%.0f\n",
                    controlledEntity->client->pers.netname,
                    m_vTargetPos.x,
                    m_vTargetPos.y,
                    m_vTargetPos.z,
                    initialDist
                );
            }
        } else {
            // Same destination, check if we're making progress
            float currentDist = (controlledEntity->origin - m_vTargetPos).length();

            // Allow some tolerance (32 units) to avoid noise from small movements
            if (currentDist < m_progress.bestDist - 32.0f) {
                // Made progress
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf(
                        "BOT %s: Progress tracking - got closer: %.0f -> %.0f (best)\n",
                        controlledEntity->client->pers.netname,
                        m_progress.bestDist,
                        currentDist
                    );
                }
                m_progress.bestDist     = currentDist;
                m_progress.lastProgress = level.inttime;
            }

            // Check if we've stalled for too long (no progress for N seconds)
            int stallTime         = (int)(g_bot_progress_stall_time->value * 1000.0f);
            int timeSinceProgress = level.inttime - m_progress.lastProgress;

            // Periodic stall warning (every 3 seconds)
            if (g_bot_debug_state->integer >= 2 && timeSinceProgress > 3000) {
                static int lastWarnTime = 0;
                if (level.inttime - lastWarnTime > 3000) {
                    lastWarnTime = level.inttime;
                    gi.Printf(
                        "BOT %s: Progress tracking - STALLED for %.1fs, dist=%.0f, best=%.0f, timeout=%.1fs\n",
                        controlledEntity->client->pers.netname,
                        timeSinceProgress / 1000.0f,
                        currentDist,
                        m_progress.bestDist,
                        stallTime / 1000.0f
                    );
                }
            }

            if (timeSinceProgress > stallTime) {
                // Stalled too long - give up and mark as blocked
                if (g_bot_debug_state->integer) {
                    gi.Printf(
                        "BOT %s: Progress tracking - GAVE UP after %.1fs, marking zone at (%.0f, %.0f) as blocked\n",
                        controlledEntity->client->pers.netname,
                        timeSinceProgress / 1000.0f,
                        m_vTargetPos.x,
                        m_vTargetPos.y
                    );
                }
                m_vBlockedDest = m_vTargetPos;
                m_bGaveUpPath  = true;
                ClearMove();
                return;
            }
        }
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

    if (m_blocked.state == 2 && level.inttime >= m_blocked.time + 750) {
        m_blocked.state = 0;

        PathSearchParameter parameters;
        parameters.entity     = controlledEntity;
        parameters.fallHeight = maxFallHeight;
        m_pPath->FindPath(controlledEntity->origin, m_vTargetPos, parameters);

        m_iLastMoveTime  = level.inttime;
        m_iCheckPathTime = level.inttime;
    }

    vDelta = m_pPath->GetCurrentDelta();
    vDelta = FixDeltaFromCollision(vDelta);

    if (m_pPath->GetNodeCount()) {
        m_pPath->UpdatePos(controlledEntity->origin);

        m_vCurrentGoal = controlledEntity->origin;
        VectorAdd2D(m_vCurrentGoal, vDelta, m_vCurrentGoal);

        if (MoveDone()) {
            // Fixed in OPM
            //  Use ClearMove() to properly reset all movement state.
            //  Previously only the path was cleared, leaving m_bPathing
            //  true. This caused states to think the bot was still moving
            //  and never issue new movement commands, trapping the bot in
            //  random micro-movements from the fallback below.
            ClearMove();
        }
    }

    if (ai_debugpath->integer) {
        G_DebugLine(controlledEntity->centroid, m_vCurrentGoal + Vector(0, 0, 36), 1, 1, 0, 1);
    }

    // Check if we're blocked
    if (level.inttime >= m_iCheckPathTime + 1000 && m_blocked.state != 2) {
        bool blocked = false;

        m_iCheckPathTime = level.inttime;

        if (m_blocked.numBlocks >= 5) {
            // Added in OPM
            //  Store blocked destination before clearing so belief map can mark it
            if (g_bot_debug_state->integer) {
                gi.Printf(
                    "BOT %s: Block detection - GAVE UP after %d blocks, marking zone at (%.0f, %.0f) as blocked\n",
                    controlledEntity->client->pers.netname,
                    m_blocked.numBlocks,
                    m_vTargetPos.x,
                    m_vTargetPos.y
                );
            }
            m_vBlockedDest = m_vTargetPos;
            m_bGaveUpPath  = true;
            // Give up
            ClearMove();
        }

        if (!m_pPath->IsQuerying() && !controlledEntity->GetLadder()) {
            if (controlledEntity->GetMoveResult() >= MOVERESULT_BLOCKED
                || controlledEntity->velocity.lengthSquared() <= Square(8)) {
                blocked = true;
            } else if ((controlledEntity->origin - m_vLastCheckPos[0]).lengthSquared() <= Square(64)
                       && (controlledEntity->origin - m_vLastCheckPos[1]).lengthSquared() <= Square(64)) {
                blocked = true;
            }
        }

        if (!blocked) {
            m_blocked.state     = 0;
            m_blocked.numBlocks = 0;

            if (!m_pPath->GetNodeCount()) {
                m_vTargetPos   = controlledEntity->origin + Vector(G_CRandom(512), G_CRandom(512), G_CRandom(512));
                m_vCurrentGoal = m_vTargetPos;
            }
        } else if (m_blocked.state == 0) {
            m_blocked.lastTime = level.inttime;
            m_blocked.state    = 1;
        }

        if (m_blocked.state && level.inttime >= m_blocked.lastTime + 1000) {
            Vector delta;
            Vector dir;

            m_blocked.state = 2;
            m_blocked.time  = level.inttime;
            m_blocked.numBlocks++;

            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Block detection - blocked %d/5 times\n",
                    controlledEntity->client->pers.netname,
                    m_blocked.numBlocks
                );
            }

            // Try to backward a little
            if (m_pPath->GetNodeCount()) {
                delta = m_pPath->GetCurrentDelta();
            } else {
                delta = m_vTargetPos - controlledEntity->origin;
            }

            m_pPath->Clear();

            if (m_blocked.numBlocks < 2) {
                dir   = -delta;
                dir.z = 0;
                dir.normalize();

                if (dir.x < -0.5 || dir.x > 0.5) {
                    dir.x *= 4;
                    dir.y /= 4;
                } else if (dir.y < -0.5 || dir.y > 0.5) {
                    dir.x /= 4;
                    dir.y *= 4;
                } else {
                    dir.x = G_CRandom(2);
                    dir.y = G_CRandom(2);
                }

                m_vCurrentGoal = controlledEntity->origin + delta + dir * 128;
            } else {
                m_vCurrentGoal = controlledEntity->origin + Vector(G_CRandom(512), G_CRandom(512), G_CRandom(512));
            }
        }

        m_vLastCheckPos[1] = m_vLastCheckPos[0];
        m_vLastCheckPos[0] = controlledEntity->origin;
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

    if (m_pPath->GetNodeCount() || m_blocked.state != 0) {
        if ((m_vTargetPos - controlledEntity->origin).lengthSquared() <= Square(16)) {
            ClearMove();
        }
    } else {
        //if ((m_vTargetPos - controlledEntity->origin).lengthXYSquared() <= Square(16)) {
        ClearMove();
        //}
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

/*
====================
AvoidPath

Avoid the specified position within the radius and start from a direction
====================
*/
void BotMovement::AvoidPath(
    Vector vAvoid, float fAvoidRadius, Vector vPreferredDir, float *vLeashHome, float fLeashRadius
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

    m_pPath->FindPathAway(controlledEntity->origin, vAvoid, vDir, fAvoidRadius, parameters);

    NewMove();

    if (!m_pPath->GetNodeCount()) {
        // Random movements
        m_vTargetPos = controlledEntity->origin + Vector(G_Random(256) - 128, G_Random(256) - 128, G_Random(256) - 128);
        m_vCurrentGoal = m_vTargetPos;
        return;
    }

    m_iLastMoveTime = level.inttime;
    m_vTargetPos    = m_pPath->GetDestination();
}

/*
====================
MoveNear

Move near the specified position within the radius
====================
*/
void BotMovement::MoveNear(Vector vNear, float fRadius, float *vLeashHome, float fLeashRadius)
{
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

    m_pPath->FindPathNear(controlledEntity->origin, vNear, fRadius, parameters);
    NewMove();

    if (!m_pPath->GetNodeCount()) {
        m_bPathing = false;
        return;
    }

    m_iLastMoveTime = level.inttime;
    m_vTargetPos    = m_pPath->GetDestination();
}

/*
====================
MoveTo

Move to the specified position
====================
*/
void BotMovement::MoveTo(Vector vPos, float *vLeashHome, float fLeashRadius)
{
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

    m_pPath->FindPath(controlledEntity->origin, vPos, parameters);

    NewMove();

    if (!m_pPath->GetNodeCount()) {
        m_bPathing = false;
        return;
    }

    m_iLastMoveTime = level.inttime;
    CheckEndPos(controlledEntity);
}

/*
====================
MoveToBestAttractivePoint

Move to the nearest attractive point with a minimum priority
Returns true if no attractive point was found
====================
*/
bool BotMovement::MoveToBestAttractivePoint(const BotBeliefMap *beliefMap, int iMinPriority)
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

        // Added in OPM
        //  Skip attractive nodes in path-blocked zones
        if (beliefMap && beliefMap->IsPathBlocked(node->origin)) {
            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Skipping attractive node at (%.0f, %.0f) - zone is path-blocked\n",
                    controlledEntity->client->pers.netname,
                    node->origin.x,
                    node->origin.y
                );
            }
            continue;
        }

        // Added in OPM
        //  Skip attractive nodes near failed targets
        if (beliefMap && beliefMap->IsNearFailedTarget(node->origin)) {
            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Skipping attractive node at (%.0f, %.0f) - near failed target\n",
                    controlledEntity->client->pers.netname,
                    node->origin.x,
                    node->origin.y
                );
            }
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
    m_bPathing         = true;
    m_vLastCheckPos[0] = controlledEntity->origin;
    m_vLastCheckPos[1] = controlledEntity->origin;
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
bool BotMovement::CanMoveTo(Vector vPos) const
{
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

    if (m_blocked.state != 0) {
        return false;
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

/*
====================
ClearMove

Stop the bot from moving
====================
*/
void BotMovement::ClearMove()
{
    m_bPathing = false;
    m_blocked.reset();
    m_progress.reset();

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
    return m_pPath->GetCurrentDirection();
}

// Added in OPM
//  Path blocking accessors
bool BotMovement::DidGiveUpPath() const
{
    return m_bGaveUpPath;
}

Vector BotMovement::GetBlockedDestination() const
{
    return m_vBlockedDest;
}

void BotMovement::ClearGiveUpFlag()
{
    m_bGaveUpPath = false;
}
