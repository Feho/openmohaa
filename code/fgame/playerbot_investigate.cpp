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
extern cvar_t *g_bot_investigate_sound_timeout;
extern cvar_t *g_bot_debug;

/*
====================
Investigate state

When a bot loses sight of an enemy, it remembers the last known position
and investigates that location, searching nearby areas in a pattern.
====================
*/
// TODO: Add unit tests for state machine transitions (Phase 2)
//  Current test coverage is incomplete for investigation state switching logic
//  Need tests for:
//  - Transition from Attack to Investigate when enemy is lost
//  - Memory expiration and state exit
//  - Investigation timeout behavior
//  - Priority handling between Investigate and Curious states
//  - Search pattern calculation for different enemy positions
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

    // Check for enemy tracking mode (original behavior)
    bool hasEnemyMemory = false;
    if (memoryState.enemyMemory.enemy) {
        // Check if memory is still fresh
        float timeSinceSeen  = (level.svsTime - memoryState.enemyMemory.lastSeenTime) * 0.001f;
        float memoryDuration = g_bot_memory_duration->value;

        if (timeSinceSeen >= memoryDuration) {
            // Memory expired, clear it
            if (g_bot_debug->integer >= 1) {
                gi.Printf(
                    "[BOT] %s: Enemy memory expired (%.1fs since last seen)\n",
                    controlledEnt->client->pers.netname,
                    timeSinceSeen
                );
            }
            memoryState.enemyMemory.enemy = NULL;
        } else {
            // Check if investigation timeout has expired
            if (memoryState.investigateStartTime > 0) {
                float investigateTime = (level.svsTime - memoryState.investigateStartTime) * 0.001f;
                if (investigateTime >= g_bot_investigate_timeout->value) {
                    // Give up enemy investigation
                    if (g_bot_debug->integer >= 1) {
                        gi.Printf(
                            "[BOT] %s: Enemy investigation timeout (%.1fs), giving up\n",
                            controlledEnt->client->pers.netname,
                            investigateTime
                        );
                    }
                    memoryState.enemyMemory.enemy = NULL;
                } else {
                    hasEnemyMemory = true;
                }
            } else {
                hasEnemyMemory = true;
            }
        }
    }

    // Check for sound investigation mode (new behavior)
    bool hasSoundInvestigation = false;
    if (memoryState.investigateEventTime > 0) {
        float timeSinceEvent = (level.svsTime - memoryState.investigateEventTime) * 0.001f;
        float soundTimeout   = g_bot_investigate_sound_timeout->value;

        if (timeSinceEvent >= soundTimeout) {
            // Sound investigation timeout
            if (g_bot_debug->integer >= 2) {
                gi.Printf(
                    "[BOT] %s: Sound investigation timeout (%.1fs), giving up\n",
                    controlledEnt->client->pers.netname,
                    timeSinceEvent
                );
            }
            memoryState.investigateEventTime = 0;
            memoryState.investigateEventPos  = vec_zero;
        } else {
            hasSoundInvestigation = true;
        }
    }

    // Investigation is active if either mode is active
    return hasEnemyMemory || hasSoundInvestigation;
}

void BotController::State_EndInvestigate(void)
{
    if (g_bot_debug->integer >= 1) {
        gi.Printf(
            "[BOT] %s: Ending investigation (searched %d positions)\n",
            controlledEnt->client->pers.netname,
            memoryState.enemyMemory.searchAttempts
        );
    }

    // Clear enemy tracking investigation state
    memoryState.enemyMemory.investigationStarted = false;
    memoryState.enemyMemory.searchAttempts       = 0;
    memoryState.investigateStartTime             = 0;

    // Clear sound investigation state
    memoryState.investigateEventTime = 0;
    memoryState.investigateEventPos  = vec_zero;

    // Reset event priority (Investigation state is ending)
    if (memoryState.currentEventPriority == 2) {
        memoryState.currentEventPriority = 0;
    }
}

Vector BotController::CalculateSearchPosition(void)
{
    const float radius = g_bot_investigate_radius->value;
    Vector      searchPos;

    // Search pattern based on attempt number
    // 0: Last known position
    // 1-4: Cardinal directions (N, E, S, W) at 256 units
    // 5-8: Diagonal directions (NE, SE, SW, NW) at 256 units
    // 9-12: Cardinal directions at 512 units

    switch (memoryState.enemyMemory.searchAttempts) {
    case 0:
        // Go to last known position
        return memoryState.enemyMemory.lastKnownPosition;

    case 1: // North
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(256, 0, 0);
        break;
    case 2: // East
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(0, 256, 0);
        break;
    case 3: // South
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(-256, 0, 0);
        break;
    case 4: // West
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(0, -256, 0);
        break;

    case 5: // NE
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(181, 181, 0);
        break;
    case 6: // SE
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(-181, 181, 0);
        break;
    case 7: // SW
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(-181, -181, 0);
        break;
    case 8: // NW
        searchPos = memoryState.enemyMemory.lastKnownPosition + Vector(181, -181, 0);
        break;

    default: // Extended search at max radius
        {
            // Random positions around last known location
            float angle = G_Random(360) * M_PI / 180.0f;
            float dist  = radius * 0.75f + G_Random(radius * 0.25f);
            searchPos   = memoryState.enemyMemory.lastKnownPosition + Vector(cos(angle) * dist, sin(angle) * dist, 0);
        }
        break;
    }

    return searchPos;
}

void BotController::State_Investigate(void)
{
    // Determine investigation mode
    bool hasEnemyMemory        = (memoryState.enemyMemory.enemy != NULL);
    bool hasSoundInvestigation = (memoryState.investigateEventTime > 0);

    // Check if we can see an enemy now (re-acquisition for both modes)
    if (CheckCondition_Attack()) {
        // Found an enemy, attack state will take over
        if (g_bot_debug->integer >= 1) {
            gi.Printf("[BOT] %s: Acquired target during investigation!\n", controlledEnt->client->pers.netname);
        }
        return;
    }

    // Handle sound investigation mode
    if (hasSoundInvestigation && !hasEnemyMemory) {
        // Sound investigation - move directly to sound location
        if (!movement.IsMoving() || movement.MoveDone()) {
            if (movement.CanMoveTo(memoryState.investigateEventPos)) {
                movement.MoveNear(memoryState.investigateEventPos, BotConstants::OBSTACLE_AVOIDANCE_DISTANCE);

                if (g_bot_debug->integer >= 2) {
                    gi.Printf(
                        "[BOT] %s: Investigating sound at (%.0f, %.0f, %.0f)\n",
                        controlledEnt->client->pers.netname,
                        memoryState.investigateEventPos.x,
                        memoryState.investigateEventPos.y,
                        memoryState.investigateEventPos.z
                    );
                }
            } else {
                // Can't reach sound location, give up
                if (g_bot_debug->integer >= 2) {
                    gi.Printf(
                        "[BOT] %s: Can't reach sound investigation position\n", controlledEnt->client->pers.netname
                    );
                }
                memoryState.investigateEventTime = 0;
                memoryState.investigateEventPos  = vec_zero;
                return;
            }
        }

        // Aim at sound location while moving
        rotation.AimAt(memoryState.investigateEventPos);
        return;
    }

    // Handle enemy tracking mode (original behavior)
    if (hasEnemyMemory) {
        // Initialize investigation on first frame
        if (memoryState.investigateStartTime == 0) {
            memoryState.investigateStartTime             = level.svsTime;
            memoryState.enemyMemory.investigationStarted = true;
            memoryState.enemyMemory.searchAttempts       = 0;

            if (g_bot_debug->integer >= 1) {
                const char *enemyName =
                    memoryState.enemyMemory.enemy->IsSubclassOfPlayer()
                        ? static_cast<Player *>(memoryState.enemyMemory.enemy.Pointer())->client->pers.netname
                        : "unknown";
                gi.Printf(
                    "[BOT] %s: Starting enemy investigation for %s at (%.0f, %.0f, %.0f)\n",
                    controlledEnt->client->pers.netname,
                    enemyName,
                    memoryState.enemyMemory.lastKnownPosition.x,
                    memoryState.enemyMemory.lastKnownPosition.y,
                    memoryState.enemyMemory.lastKnownPosition.z
                );
            }
        }

        // Move to search positions
        if (movement.MoveDone()) {
            // Calculate next search position
            Vector searchPos = CalculateSearchPosition();

            // Try to pathfind to the search position
            if (movement.CanMoveTo(searchPos)) {
                movement.MoveNear(searchPos, BotConstants::OBSTACLE_AVOIDANCE_DISTANCE);
                memoryState.enemyMemory.searchAttempts++;

                if (g_bot_debug->integer >= 2) {
                    gi.Printf(
                        "[BOT] %s: Searching enemy position %d at (%.0f, %.0f, %.0f)\n",
                        controlledEnt->client->pers.netname,
                        memoryState.enemyMemory.searchAttempts,
                        searchPos.x,
                        searchPos.y,
                        searchPos.z
                    );
                }
            } else {
                // Can't reach this position, try next one
                memoryState.enemyMemory.searchAttempts++;

                if (g_bot_debug->integer >= 2) {
                    gi.Printf(
                        "[BOT] %s: Can't reach enemy search position %d, skipping\n",
                        controlledEnt->client->pers.netname,
                        memoryState.enemyMemory.searchAttempts
                    );
                }

                if (memoryState.enemyMemory.searchAttempts > 12) {
                    // Exhausted search attempts, give up
                    if (g_bot_debug->integer >= 1) {
                        gi.Printf(
                            "[BOT] %s: Exhausted all enemy search positions\n", controlledEnt->client->pers.netname
                        );
                    }
                    memoryState.enemyMemory.enemy = NULL;
                    return;
                }
            }
        }

        // Aim at last known position while moving
        rotation.AimAt(memoryState.enemyMemory.lastKnownPosition);

        // Decay confidence over time
        float timeSinceSeen                     = (level.svsTime - memoryState.enemyMemory.lastSeenTime) * 0.001f;
        memoryState.enemyMemory.confidenceLevel = 1.0f - (timeSinceSeen / g_bot_memory_duration->value);
    }
}
