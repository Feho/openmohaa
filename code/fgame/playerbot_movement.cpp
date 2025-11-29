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

static int maxFallHeight = 400;

BotMovement::BotMovement()
{
    controlledEntity = NULL;

    m_pPath         = NULL;
    m_iLastMoveTime = 0;

    m_bPathing       = false;
    m_iTempAwayState = 0;
    m_fAttractTime   = 0;

    m_iCheckPathTime = 0;
    m_iTempAwayTime  = 0;
    m_iNumBlocks     = 0;

    m_bAvoidCollision     = false;
    m_iCollisionCheckTime = 0;
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

    botcmd.forwardmove = 0;
    botcmd.rightmove   = 0;

    CheckAttractiveNodes();

    if (!IsMoving() || !m_pPath) {
        return;
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

    if (m_iTempAwayState == 2 && level.inttime >= m_iTempAwayTime + 750) {
        m_iTempAwayState = 0;

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
            // Clear the path
            m_pPath->Clear();
        }
    }

    if (ai_debugpath->integer) {
        G_DebugLine(controlledEntity->centroid, m_vCurrentGoal + Vector(0, 0, 36), 1, 1, 0, 1);
    }

    // Check if we're blocked
    if (level.inttime >= m_iCheckPathTime + 1000 && m_iTempAwayState != 2) {
        bool blocked = false;

        m_iCheckPathTime = level.inttime;

        if (m_iNumBlocks >= 5) {
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
            m_iTempAwayState = 0;
            m_iNumBlocks     = 0;

            if (!m_pPath->GetNodeCount()) {
                m_vTargetPos   = controlledEntity->origin + Vector(G_CRandom(512), G_CRandom(512), G_CRandom(512));
                m_vCurrentGoal = m_vTargetPos;
            }
        } else if (m_iTempAwayState == 0) {
            m_iLastBlockTime = level.inttime;
            m_iTempAwayState = 1;
        }

        if (m_iTempAwayState && level.inttime >= m_iLastBlockTime + 1000) {
            Vector delta;
            Vector dir;

            m_iTempAwayState = 2;
            m_iTempAwayTime  = level.inttime;
            m_iNumBlocks++;

            // Try to backward a little
            if (m_pPath->GetNodeCount()) {
                delta = m_pPath->GetCurrentDelta();
            } else {
                delta = m_vTargetPos - controlledEntity->origin;
            }

            m_pPath->Clear();

            if (m_iNumBlocks < 2) {
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

    if (m_pPath->GetNodeCount() || m_iTempAwayState != 0) {
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

    if (!m_bJump) {
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

/*
====================
MoveDone

Returns true if the bot has done moving
====================
*/
bool BotMovement::MoveDone()
{
    if (!m_bPathing) {
        return true;
    }

    if (m_iTempAwayState != 0) {
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
bool BotMovement::IsMoving(void)
{
    return m_bPathing;
}

/*
====================
ClearMove

Stop the bot from moving
====================
*/
void BotMovement::ClearMove(void)
{
    m_bPathing   = false;
    m_iNumBlocks = 0;

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
