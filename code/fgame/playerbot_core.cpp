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
// playerbot_core.cpp: Core initialization and lifecycle management for bot controller

#include "g_local.h"
#include "playerbot.h"

cvar_t *bot_manualmove;

CLASS_DECLARATION(Listener, BotController, NULL) {
    {NULL, NULL}
};

BotController::botfunc_t BotController::botfuncs[MAX_BOT_FUNCTIONS];

BotController::BotController()
{
    if (LoadingSavegame) {
        return;
    }

    m_botCmd.serverTime = 0;
    m_botCmd.msec       = 0;
    m_botCmd.buttons    = 0;
    m_botCmd.angles[0]  = ANGLE2SHORT(0);
    m_botCmd.angles[1]  = ANGLE2SHORT(0);
    m_botCmd.angles[2]  = ANGLE2SHORT(0);

    m_botCmd.forwardmove = 0;
    m_botCmd.rightmove   = 0;
    m_botCmd.upmove      = 0;

    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;
    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = DEFAULT_VIEWHEIGHT;

    m_iCuriousTime        = 0;
    m_iAttackTime         = 0;
    m_iEnemyEyesTag       = -1;
    m_iContinuousFireTime = 0;
    m_iLastSeenTime       = 0;
    m_iLastUnseenTime     = 0;
    m_iLastBurstTime      = 0;

    m_iNextTauntTime = 0;

    // Initialize state entry times and target lock time
    memset(m_iStateEntryTime, 0, sizeof(m_iStateEntryTime));
    m_iTargetLockTime = 0;

    // Initialize enemy memory
    m_enemyMemory.enemy = NULL;
    m_enemyMemory.lastKnownPosition = vec_zero;
    m_enemyMemory.lastKnownVelocity = vec_zero;
    m_enemyMemory.lastSeenTime = 0.0f;
    m_enemyMemory.confidenceLevel = 0.0f;
    m_enemyMemory.investigationStarted = false;
    m_enemyMemory.searchAttempts = 0;
    m_iInvestigateStartTime = 0;

    // Initialize sound-based investigation tracking
    m_iInvestigateEventTime = 0;
    m_vInvestigateEventPos = vec_zero;
    m_iCurrentEventPriority = 0;

    // Initialize cover system
    m_currentCover.position = vec_zero;
    m_currentCover.quality = 0.0f;
    m_currentCover.protectionAngle = 0.0f;
    m_currentCover.distanceToEnemy = 0.0f;
    m_currentCover.hasEscapeRoute = false;
    m_currentCover.evaluatedTime = 0;
    m_coverState = COVER_NONE;
    m_iNextPeekTime = 0;
    m_iPeekStartTime = 0;
    m_fPeekDuration = 0.0f;
    m_iLastCoverSearchTime = 0;

    // Initialize tactical combat system
    m_fireMode = FIRE_BURST;
    m_combatProfile = CAUTIOUS;
    m_iSuppressionEndTime = 0;
    m_fRecentDamage = 0.0f;
    m_iDamageWindowStart = 0;
    m_fBurstDuration = 1.0f;
    m_fBurstDelay = 0.5f;
    m_bRequireLowSpread = false;
    m_bAmmoLow = false;

    // Initialize squad coordination system
    m_squad.sharedTarget = NULL;
    m_squad.rallyPoint = vec_zero;
    m_squad.lastUpdate = 0;
    m_squadRole = ROLE_NONE;
    m_iLastSquadUpdateTime = 0;
    m_iRoleAssignmentTime = 0;
    m_vFlankPosition = vec_zero;
    m_bFlankPositionValid = false;

    m_StateFlags = 0;
}

BotController::~BotController()
{
    if (controlledEnt) {
        controlledEnt->delegate_gotKill.Remove(delegateHandle_gotKill);
        controlledEnt->delegate_killed.Remove(delegateHandle_killed);
        controlledEnt->delegate_stufftext.Remove(delegateHandle_stufftext);
        controlledEnt->delegate_spawned.Remove(delegateHandle_spawned);
    }
}

void BotController::Init(void)
{
    bot_manualmove = gi.Cvar_Get("bot_manualmove", "0", 0);

    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        botfuncs[i].BeginState = &BotController::State_DefaultBegin;
        botfuncs[i].EndState   = &BotController::State_DefaultEnd;
    }

    InitState_Attack(&botfuncs[0]);
    InitState_Investigate(&botfuncs[1]);
    InitState_Curious(&botfuncs[2]);
    InitState_Grenade(&botfuncs[3]);
    InitState_Idle(&botfuncs[4]);
}

BotMovement& BotController::GetMovement()
{
    return movement;
}

void BotController::GetUsercmd(usercmd_t *ucmd)
{
    *ucmd = m_botCmd;
}

void BotController::GetEyeInfo(usereyes_t *eyeinfo)
{
    *eyeinfo = m_botEyes;
}

void BotController::setControlledEntity(Player *player)
{
    controlledEnt = player;
    movement.SetControlledEntity(player);
    rotation.SetControlledEntity(player);

    delegateHandle_gotKill =
        player->delegate_gotKill.Add(std::bind(&BotController::GotKill, this, std::placeholders::_1));
    delegateHandle_killed = player->delegate_killed.Add(std::bind(&BotController::Killed, this, std::placeholders::_1));
    delegateHandle_stufftext =
        player->delegate_stufftext.Add(std::bind(&BotController::EventStuffText, this, std::placeholders::_1));
    delegateHandle_spawned = player->delegate_spawned.Add(std::bind(&BotController::Spawned, this));
}

Player *BotController::getControlledEntity() const
{
    return controlledEnt;
}
