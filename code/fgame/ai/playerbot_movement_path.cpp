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
// playerbot_movement_path.cpp: Manages bot pathfinding

#include "playerbot.h"

static int maxFallHeight = 400;

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
CanMoveTo

Returns true if the bot has done moving
====================
*/
bool BotMovement::CanMoveTo(Vector vPos)
{
    PathSearchParameter parameters;
    parameters.fallHeight = maxFallHeight;
    parameters.entity     = controlledEntity;
    return m_pPath->TestPath(controlledEntity->origin, vPos, parameters);
}
