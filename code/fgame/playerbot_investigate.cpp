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
// playerbot_investigate.cpp: Investigation state for tracking lost enemies

#include "g_local.h"
#include "playerbot.h"

extern cvar_t *g_bot_memory_duration;
extern cvar_t *g_bot_investigate_radius;
extern cvar_t *g_bot_investigate_timeout;
extern cvar_t *g_bot_debug;

/*
====================
Investigate state

When a bot loses sight of an enemy, it remembers the last known position
and investigates that location, searching nearby areas in a pattern.
====================
*/
void BotController::InitState_Investigate(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Investigate;
    func->EndState       = &BotController::State_EndInvestigate;
    func->ThinkState     = &BotController::State_Investigate;
}

bool BotController::CheckCondition_Investigate(void)
{
    // Don't investigate if we have a visible enemy
    if (m_pEnemy) {
        return false;
    }

    // Check if we have enemy memory
    if (!m_enemyMemory.enemy) {
        return false;
    }

    // Check if memory is still fresh
    float timeSinceSeen = level.svsTime - m_enemyMemory.lastSeenTime;
    float memoryDuration = g_bot_memory_duration->value;

    if (timeSinceSeen >= memoryDuration) {
        // Memory expired, clear it
        if (g_bot_debug->integer >= 1) {
            gi.Printf("[BOT] %s: Memory expired (%.1fs since last seen)\n",
                controlledEnt->client->pers.netname, timeSinceSeen);
        }
        m_enemyMemory.enemy = NULL;
        return false;
    }

    // Check if investigation timeout has expired
    if (m_iInvestigateStartTime > 0) {
        float investigateTime = (level.svsTime - m_iInvestigateStartTime);
        if (investigateTime >= g_bot_investigate_timeout->value) {
            // Give up investigation
            if (g_bot_debug->integer >= 1) {
                gi.Printf("[BOT] %s: Investigation timeout (%.1fs), giving up\n",
                    controlledEnt->client->pers.netname, investigateTime);
            }
            m_enemyMemory.enemy = NULL;
            return false;
        }
    }

    // Active investigation
    return true;
}

void BotController::State_EndInvestigate(void)
{
    if (g_bot_debug->integer >= 1) {
        gi.Printf("[BOT] %s: Ending investigation (searched %d positions)\n",
            controlledEnt->client->pers.netname, m_enemyMemory.searchAttempts);
    }

    // Clear investigation state
    m_enemyMemory.investigationStarted = false;
    m_enemyMemory.searchAttempts = 0;
    m_iInvestigateStartTime = 0;
}

Vector BotController::CalculateSearchPosition(void)
{
    const float radius = g_bot_investigate_radius->value;
    Vector searchPos;

    // Search pattern based on attempt number
    // 0: Last known position
    // 1-4: Cardinal directions (N, E, S, W) at 256 units
    // 5-8: Diagonal directions (NE, SE, SW, NW) at 256 units
    // 9-12: Cardinal directions at 512 units

    switch (m_enemyMemory.searchAttempts) {
        case 0:
            // Go to last known position
            return m_enemyMemory.lastKnownPosition;

        case 1: // North
            searchPos = m_enemyMemory.lastKnownPosition + Vector(256, 0, 0);
            break;
        case 2: // East
            searchPos = m_enemyMemory.lastKnownPosition + Vector(0, 256, 0);
            break;
        case 3: // South
            searchPos = m_enemyMemory.lastKnownPosition + Vector(-256, 0, 0);
            break;
        case 4: // West
            searchPos = m_enemyMemory.lastKnownPosition + Vector(0, -256, 0);
            break;

        case 5: // NE
            searchPos = m_enemyMemory.lastKnownPosition + Vector(181, 181, 0);
            break;
        case 6: // SE
            searchPos = m_enemyMemory.lastKnownPosition + Vector(-181, 181, 0);
            break;
        case 7: // SW
            searchPos = m_enemyMemory.lastKnownPosition + Vector(-181, -181, 0);
            break;
        case 8: // NW
            searchPos = m_enemyMemory.lastKnownPosition + Vector(181, -181, 0);
            break;

        default: // Extended search at max radius
            {
                // Random positions around last known location
                float angle = G_Random(360) * M_PI / 180.0f;
                float dist = radius * 0.75f + G_Random(radius * 0.25f);
                searchPos = m_enemyMemory.lastKnownPosition + Vector(cos(angle) * dist, sin(angle) * dist, 0);
            }
            break;
    }

    return searchPos;
}

void BotController::State_Investigate(void)
{
    if (!m_enemyMemory.enemy) {
        return;
    }

    // Initialize investigation on first frame
    if (m_iInvestigateStartTime == 0) {
        m_iInvestigateStartTime = level.svsTime;
        m_enemyMemory.investigationStarted = true;
        m_enemyMemory.searchAttempts = 0;

        if (g_bot_debug->integer >= 1) {
            const char* enemyName = m_enemyMemory.enemy->IsSubclassOfPlayer()
                ? static_cast<Player*>(m_enemyMemory.enemy.Pointer())->client->pers.netname
                : "unknown";
            gi.Printf("[BOT] %s: Starting investigation for %s at (%.0f, %.0f, %.0f)\n",
                controlledEnt->client->pers.netname, enemyName,
                m_enemyMemory.lastKnownPosition.x,
                m_enemyMemory.lastKnownPosition.y,
                m_enemyMemory.lastKnownPosition.z);
        }
    }

    // Check if we can see the enemy now (re-acquisition)
    if (CheckCondition_Attack()) {
        // Found the enemy again, attack state will take over
        if (g_bot_debug->integer >= 1) {
            gi.Printf("[BOT] %s: Re-acquired target during investigation!\n",
                controlledEnt->client->pers.netname);
        }
        return;
    }

    // Move to search positions
    if (movement.MoveDone()) {
        // Calculate next search position
        Vector searchPos = CalculateSearchPosition();

        // Try to pathfind to the search position
        if (movement.CanMoveTo(searchPos)) {
            movement.MoveNear(searchPos, 128.0f);
            m_enemyMemory.searchAttempts++;

            if (g_bot_debug->integer >= 2) {
                gi.Printf("[BOT] %s: Searching position %d at (%.0f, %.0f, %.0f)\n",
                    controlledEnt->client->pers.netname, m_enemyMemory.searchAttempts,
                    searchPos.x, searchPos.y, searchPos.z);
            }
        } else {
            // Can't reach this position, try next one
            m_enemyMemory.searchAttempts++;

            if (g_bot_debug->integer >= 2) {
                gi.Printf("[BOT] %s: Can't reach search position %d, skipping\n",
                    controlledEnt->client->pers.netname, m_enemyMemory.searchAttempts);
            }

            if (m_enemyMemory.searchAttempts > 12) {
                // Exhausted search attempts, give up
                if (g_bot_debug->integer >= 1) {
                    gi.Printf("[BOT] %s: Exhausted all search positions\n",
                        controlledEnt->client->pers.netname);
                }
                m_enemyMemory.enemy = NULL;
                return;
            }
        }
    }

    // Aim at last known position while moving
    rotation.AimAt(m_enemyMemory.lastKnownPosition);

    // Decay confidence over time
    float timeSinceSeen = level.svsTime - m_enemyMemory.lastSeenTime;
    m_enemyMemory.confidenceLevel = 1.0f - (timeSinceSeen / g_bot_memory_duration->value);
}
