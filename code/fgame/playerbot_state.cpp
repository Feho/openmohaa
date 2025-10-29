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
// playerbot_state.cpp: State management system for bot controller

#include "g_local.h"
#include "playerbot.h"

extern cvar_t *g_bot_state_minduration_attack;
extern cvar_t *g_bot_state_minduration_investigate;

void BotController::CheckStates(void)
{
    m_StateCount = 0;

    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        botfunc_t *func = &botfuncs[i];

        if (func->CheckCondition) {
            bool conditionMet = (this->*func->CheckCondition)();
            bool stateActive  = (m_StateFlags & (1 << i)) != 0;

            if (conditionMet) {
                if (!stateActive) {
                    // State is becoming active - record entry time
                    m_StateFlags |= 1 << i;
                    m_iStateEntryTime[i] = level.svsTime;

                    if (func->BeginState) {
                        (this->*func->BeginState)();
                    }
                }

                if (func->ThinkState) {
                    m_StateCount++;
                    (this->*func->ThinkState)();
                }
            } else {
                if (stateActive) {
                    // Condition no longer met, but check if we can exit based on minimum duration
                    if (CanExitState(i)) {
                        // Enough time has passed, allow state exit
                        m_StateFlags &= ~(1 << i);
                        m_iStateEntryTime[i] = 0;

                        if (func->EndState) {
                            (this->*func->EndState)();
                        }
                    } else {
                        // Minimum duration not met, keep state active
                        if (func->ThinkState) {
                            m_StateCount++;
                            (this->*func->ThinkState)();
                        }
                    }
                }
            }
        } else {
            if (func->ThinkState) {
                m_StateCount++;
                (this->*func->ThinkState)();
            }
        }
    }

    assert(m_StateCount);
    if (!m_StateCount) {
        gi.DPrintf("*** WARNING *** %s was stuck with no states !!!", controlledEnt->client->pers.netname);
        State_Reset();
    }
}

bool BotController::CanExitState(int stateIndex)
{
    // Check if this state has a minimum duration requirement
    float minDuration = 0.0f;

    switch (stateIndex) {
    case 0: // Attack state
        minDuration = g_bot_state_minduration_attack->value;
        break;
    case 1: // Investigate state
        minDuration = g_bot_state_minduration_investigate->value;
        break;
    default:
        // Other states (Curious, Grenade, Idle, Weapon) have no minimum duration
        return true;
    }

    // If no minimum duration, always allow exit
    if (minDuration <= 0.0f) {
        return true;
    }

    // Check if state was never entered (should not happen, but be safe)
    if (m_iStateEntryTime[stateIndex] == 0) {
        return true;
    }

    // Check if enough time has passed
    float timeInState = (level.svsTime - m_iStateEntryTime[stateIndex]);
    return timeInState >= minDuration;
}

/*
====================
Default state


====================
*/
void BotController::State_DefaultBegin(void)
{
    movement.ClearMove();
}

void BotController::State_DefaultEnd(void) {}

void BotController::State_Reset(void)
{
    m_iCuriousTime    = 0;
    m_iAttackTime     = 0;
    m_vLastCuriousPos = vec_zero;
    m_vOldEnemyPos    = vec_zero;
    m_vLastEnemyPos   = vec_zero;
    m_vLastDeathPos   = vec_zero;
    m_pEnemy          = NULL;
    m_iEnemyEyesTag   = -1;

    // Clear enemy memory
    memoryState.enemyMemory.enemy                = NULL;
    memoryState.enemyMemory.lastKnownPosition    = vec_zero;
    memoryState.enemyMemory.lastKnownVelocity    = vec_zero;
    memoryState.enemyMemory.lastSeenTime         = 0.0f;
    memoryState.enemyMemory.confidenceLevel      = 0.0f;
    memoryState.enemyMemory.investigationStarted = false;
    memoryState.enemyMemory.searchAttempts       = 0;
    memoryState.investigateStartTime             = 0;

    // Clear cover state
    coverState.current.quality = 0.0f;
    coverState.state           = COVER_NONE;
    m_iLastCoverSearchTime     = 0;
}
