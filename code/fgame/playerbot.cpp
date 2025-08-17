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
// playerbot.cpp: Multiplayer bot system.
//
// FIXME: Refactor code and use OOP-based state system

#include "g_local.h"
#include "actor.h"
#include "playerbot.h"
#include "consoleevent.h"
#include "debuglines.h"
#include "scriptexception.h"
#include "vehicleturret.h"
#include "weaputils.h"
#include "windows.h"
#include "g_bot.h"

/********************************************************************************

Bot States

********************************************************************************/

class AttackBotState : public BotState
{
public:
    AttackBotState(BotController* pController) : BotState(pController) {}
    virtual void Think();
    virtual const char* GetName() { return "Attack"; }
};

class CuriousBotState : public BotState
{
public:
    CuriousBotState(BotController* pController) : BotState(pController) {}
    virtual void Think();
    virtual const char* GetName() { return "Curious"; }
};

class IdleBotState : public BotState
{
public:
    IdleBotState(BotController* pController) : BotState(pController) {}
    virtual void Think();
    virtual const char* GetName() { return "Idle"; }
};

class GrenadeBotState : public BotState
{
public:
    GrenadeBotState(BotController* pController) : BotState(pController) {}
    virtual void Think();
    virtual const char* GetName() { return "Grenade"; }
};

class WeaponBotState : public BotState
{
public:
    WeaponBotState(BotController* pController) : BotState(pController) {}
    virtual void Think();
    virtual const char* GetName() { return "Weapon"; }
};

void AttackBotState::Think()
{
    // Early validation - ensure we have a valid enemy
    if (!m_pController->m_pEnemy || !m_pController->IsValidEnemy(m_pController->m_pEnemy)) {
        m_pController->m_iAttackTime = 0;
        return;
    }

    // Get current weapon - bail if no weapon available
    Weapon *pWeap = m_pController->getControlledEntity()->GetActiveWeapon(WEAPON_MAIN);
    if (!pWeap) {
        return;
    }

    // Calculate distances for decision making
    const float fDistanceSquared = (m_pController->m_pEnemy->origin - m_pController->getControlledEntity()->origin).lengthSquared();
    const float fMinDistance = 128.0f;
    float fMinDistanceSquared = fMinDistance * fMinDistance;  // Not const - can be modified by PerformAttack
    
    // Update enemy position tracking
    m_pController->m_vOldEnemyPos = m_pController->m_vLastEnemyPos;

    // Determine visibility and combat capability  
    const bool bCanSee = m_pController->getControlledEntity()->CanSee(
        m_pController->m_pEnemy, 
        90,  // Use same angle as enemy detection for consistency
        Q_min(world->m_fAIVisionDistance, world->farplane_distance * 1.2), 
        false
    );

    // Combat state variables
    bool bMelee = false;
    bool bNoMove = false;
    bool bFiring = false;
    bool bCanAttack = false;

    if (bCanSee) {
        // Enemy is visible - evaluate attack capability
        bCanAttack = m_pController->CheckReactionTime(fDistanceSquared);
        bFiring = m_pController->PerformAttack(bCanSee, bCanAttack, fDistanceSquared, bMelee, bNoMove, fMinDistanceSquared, pWeap);
    } else {
        // Enemy not visible - stop attacking and track time
        m_pController->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        
        if (level.inttime > m_pController->m_iLastSeenTime + 2000) {
            m_pController->m_iLastUnseenTime = level.inttime;
        }
    }

    // Update aiming regardless of visibility (for prediction)
    m_pController->AimAtEnemy(bCanSee, fDistanceSquared, bFiring);

    // Handle movement and positioning
    const float fEnemyDistanceSquared = (m_pController->getControlledEntity()->origin - m_pController->m_vLastEnemyPos).lengthSquared();
    if (m_pController->PerformCombatMovement(bCanSee, bMelee, bNoMove, bFiring, fMinDistanceSquared, fEnemyDistanceSquared)) {
        return; // Enemy was cleared during movement
    }
}

void CuriousBotState::Think()
{
    // Ensure we're running when curious (investigating) - clear ALL combat stance
    m_pController->m_bIsCrouching = false;
    m_pController->m_bWantsToRun = true;
    m_pController->m_botCmd.buttons |= BUTTON_RUN;
    m_pController->m_botCmd.upmove = 0;  // Clear crouch
    m_pController->m_botCmd.rightmove = 0;  // Clear any strafing from combat
    
    // FORCE clear actual player crouch flags - this is the real fix!
    if (m_pController->controlledEnt && m_pController->controlledEnt->client) {
        m_pController->controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
    }
    
    if (m_pController->CheckWindows()) {
        m_pController->m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_pController->m_iLastFireTime = level.inttime;
    } else {
        m_pController->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    }

    m_pController->AimAtAimNode();

    if (!m_pController->GetMovement().MoveToBestAttractivePoint(3) && (!m_pController->GetMovement().IsMoving() || m_pController->m_vLastCuriousPos != m_pController->m_vNewCuriousPos)) {
        m_pController->GetMovement().MoveTo(m_pController->m_vNewCuriousPos);
        m_pController->m_vLastCuriousPos = m_pController->m_vNewCuriousPos;
    }

    if (m_pController->GetMovement().MoveDone()) {
        m_pController->m_iCuriousTime = 0;
    }
    
    // FINAL SAFETY: Force clear combat stance at end of non-combat state
    m_pController->m_bIsCrouching = false;
    m_pController->m_botCmd.upmove = 0;  // Normal movement (physics flags do the real work)
    // FORCE clear actual player crouch flags!
    if (m_pController->controlledEnt && m_pController->controlledEnt->client) {
        m_pController->controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
    }
}

void IdleBotState::Think()
{
    // Ensure we're not stuck crouching in idle state - clear ALL combat stance
    m_pController->m_bIsCrouching = false;
    m_pController->m_bWantsToRun = true;
    m_pController->m_botCmd.buttons |= BUTTON_RUN;
    m_pController->m_botCmd.upmove = 0;  // Normal movement (not jumping)
    m_pController->m_botCmd.rightmove = 0;  // Clear any strafing from combat
    
    // FORCE clear actual player crouch flags - this is the real fix!
    if (m_pController->controlledEnt && m_pController->controlledEnt->client) {
        m_pController->controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
    }
    
    if (m_pController->CheckWindows()) {
        m_pController->m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_pController->m_iLastFireTime = level.inttime;
    } else {
        m_pController->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        m_pController->CheckReload();
    }

    m_pController->AimAtAimNode();

    if (!m_pController->GetMovement().MoveToBestAttractivePoint() && !m_pController->GetMovement().IsMoving()) {
        // Only go to death position if we've been idle for a while (avoid immediate retreat after kills)
        const int idleTime = level.inttime - m_pController->m_iAttackTime;
        const bool hasBeenIdleLongEnough = (m_pController->m_iAttackTime == 0) || (idleTime > 5000); // 5 seconds after combat
        
        if (m_pController->m_vLastDeathPos != vec_zero && hasBeenIdleLongEnough) {
            m_pController->GetMovement().MoveTo(m_pController->m_vLastDeathPos);

            if (m_pController->GetMovement().MoveDone()) {
                m_pController->m_vLastDeathPos = vec_zero;
            }
        } else {
            Vector randomDir(G_CRandom(16), G_CRandom(16), G_CRandom(16));
            Vector preferredDir;
            float  radius = 512 + G_Random(2048);

            preferredDir += Vector(m_pController->getControlledEntity()->orientation[0]) * (rand() % 5 ? 1024 : -1024);
            preferredDir += Vector(m_pController->getControlledEntity()->orientation[2]) * (rand() % 5 ? 1024 : -1024);
            m_pController->GetMovement().AvoidPath(m_pController->getControlledEntity()->origin + randomDir, radius, preferredDir);
        }
    }
    
    // FINAL SAFETY: Force clear combat stance at end of non-combat state  
    m_pController->m_bIsCrouching = false;
    m_pController->m_botCmd.upmove = 0;  // Normal movement (physics flags do the real work)
    // FORCE clear actual player crouch flags!
    if (m_pController->controlledEnt && m_pController->controlledEnt->client) {
        m_pController->controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
    }
}

void GrenadeBotState::Think()
{
}

void WeaponBotState::Think()
{
}


// We assume that we have limited access to the server-side
// and that most logic come from the playerstate_s structure

cvar_t *bot_manualmove;

CLASS_DECLARATION(Listener, BotController, NULL) {
    {NULL, NULL}
};



BotController::BotController()
{
    // Initialize pointers to null first for safety
    m_pCurrentState = nullptr;
    m_pAttackState = nullptr;
    m_pCuriousState = nullptr;
    m_pIdleState = nullptr;
    m_pGrenadeState = nullptr;
    m_pWeaponState = nullptr;

    if (LoadingSavegame) {
        return;
    }

    // Create states in proper order
    m_pAttackState = new AttackBotState(this);
    m_pCuriousState = new CuriousBotState(this);
    m_pIdleState = new IdleBotState(this);
    m_pGrenadeState = new GrenadeBotState(this);
    m_pWeaponState = new WeaponBotState(this);
    
    // Set initial state and enter it
    m_pCurrentState = m_pIdleState;
    if (m_pCurrentState) {
        m_pCurrentState->Enter();
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
    m_iLastStateChangeTime = 0;

    m_iNextTauntTime = 0;

    //m_StateFlags = 0;
    //m_RunLabel.TrySetScript("global/bot_run.scr");
    m_fSkill = 0.01f + G_Random(0.8f);

    // Initialize strafing behavior
    m_iLastStrafeTime  = 0;
    m_iStrafeDirection = 0;
    m_iStrafeChangeTime = 0;

    // Initialize combat stance behavior
    m_iLastStanceChangeTime = 0;
    m_iCrouchTime = 0;
    m_iStandTime = 0;
    m_bIsCrouching = false;
    m_bWantsToRun = true;
    
    // Performance optimization
    m_iLastFullUpdateTime = 0;
}

BotController::~BotController()
{
    // Properly exit current state before cleanup
    if (m_pCurrentState)
    {
        m_pCurrentState->Exit();
        m_pCurrentState = nullptr;
    }

    // Clean up states in reverse order of creation
    delete m_pWeaponState;
    delete m_pGrenadeState;
    delete m_pIdleState;
    delete m_pCuriousState;
    delete m_pAttackState;
    
    // Set all pointers to null for safety
    m_pWeaponState = nullptr;
    m_pGrenadeState = nullptr;
    m_pIdleState = nullptr;
    m_pCuriousState = nullptr;
    m_pAttackState = nullptr;

    if (controlledEnt) {
        controlledEnt->delegate_gotKill.Remove(delegateHandle_gotKill);
        controlledEnt->delegate_killed.Remove(delegateHandle_killed);
        controlledEnt->delegate_stufftext.Remove(delegateHandle_stufftext);
        controlledEnt->delegate_spawned.Remove(delegateHandle_spawned);
    }
}

BotMovement& BotController::GetMovement()
{
    return movement;
}

float BotController::GetSkill() const
{
    return m_fSkill;
}

void BotController::Init(void)
{
    bot_manualmove = gi.Cvar_Get("bot_manualmove", "0", 0);
}

void BotController::GetUsercmd(usercmd_t *ucmd)
{
    *ucmd = m_botCmd;
}

void BotController::GetEyeInfo(usereyes_t *eyeinfo)
{
    *eyeinfo = m_botEyes;
}

void BotController::ChangeState(BotState* pNewState)
{
    // Don't change to the same state
    if (m_pCurrentState == pNewState) {
        return;
    }

    // Don't change to null state
    if (!pNewState) {
        gi.DPrintf("BotController::ChangeState: Attempted to change to null state\n");
        return;
    }

    if (m_pCurrentState)
    {
        // gi.DPrintf("%s: Exiting state %s\n", controlledEnt->client->pers.netname, m_pCurrentState->GetName());
        m_pCurrentState->Exit();
        
        // Clear combat stance when exiting AttackBotState to prevent stuck crouching
        if (dynamic_cast<AttackBotState*>(m_pCurrentState)) {
            m_bIsCrouching = false;
            m_bWantsToRun = true;
            m_botCmd.upmove = 0;  // Normal movement (physics flags do the real work)
            m_botCmd.buttons |= BUTTON_RUN;
            m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
            // FORCE clear actual player crouch flags!
            if (controlledEnt && controlledEnt->client) {
                controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
            }
        }
    }

    m_pCurrentState = pNewState;

    if (m_pCurrentState)
    {
        // gi.DPrintf("%s: Entering state %s\n", controlledEnt->client->pers.netname, m_pCurrentState->GetName());
        m_pCurrentState->Enter();
    }
}

void BotController::UpdateStates(void)
{
    BotState* desiredState = nullptr;
    
    // Determine the desired state based on current conditions
    if (CheckCondition_Attack())
    {
        desiredState = m_pAttackState;
    }
    else if (CheckCondition_Curious())
    {
        desiredState = m_pCuriousState;
    }
    else
    {
        desiredState = m_pIdleState;
    }
    
    // Only change state if different from current state
    if (m_pCurrentState != desiredState)
    {
        // Add state transition validation and hysteresis
        if (ShouldTransitionToState(desiredState))
        {
            ChangeState(desiredState);
        }
    }
}

/*
====================
ShouldTransitionToState

Determines if a state transition should occur, implementing hysteresis 
and transition validation to prevent rapid state switching
====================
*/
bool BotController::ShouldTransitionToState(BotState* newState)
{
    if (!newState) {
        return false;
    }
    
    // Always allow transition if no current state
    if (!m_pCurrentState) {
        return true;
    }
    
    // Get current time for transition timing
    const int currentTime = level.inttime;
    
    // Implement hysteresis - require minimum time in current state before switching
    // This prevents rapid state switching that can make bots look erratic
    const int minStateTime = 1000; // 1 second minimum
    
    if (currentTime < m_iLastStateChangeTime + minStateTime) {
        // Too soon to change states - stay in current state for stability
        return false;
    }
    
    // Special transition rules for better behavior
    if (m_pCurrentState == m_pAttackState) {
        // When leaving attack state, require longer delay to avoid rapid re-engaging
        if (newState != m_pAttackState && currentTime < m_iLastStateChangeTime + 2000) {
            return false;
        }
    }
    
    // Allow the transition and update timing
    m_iLastStateChangeTime = currentTime;
    return true;
}

void BotController::UpdateBotStates(void)
{
    // Early validation - ensure we have a valid controlled entity
    if (!controlledEnt || !controlledEnt->client) {
        gi.DPrintf("BotController::UpdateBotStates: Invalid controlled entity\n");
        return;
    }

    if (bot_manualmove->integer) {
        memset(&m_botCmd, 0, sizeof(usercmd_t));
        return;
    }

    m_botCmd.serverTime = level.svsTime;

    // Ensure bot has a primary weapon assigned
    if (!controlledEnt->client->pers.dm_primary[0]) {
        Event *event = new Event(EV_Player_PrimaryDMWeapon);
        event->AddString("auto");
        controlledEnt->ProcessEvent(event);
    }

    if (controlledEnt->GetTeam() == TEAM_NONE || controlledEnt->GetTeam() == TEAM_SPECTATOR) {
        float time;

        // Add some delay to avoid telefragging
        time = controlledEnt->entnum / 20.0;

        if (controlledEnt->EventPending(EV_Player_AutoJoinDMTeam)) {
            return;
        }

        //
        // Team
        //
        controlledEnt->PostEvent(EV_Player_AutoJoinDMTeam, time);
        return;
    }

    if (controlledEnt->IsDead() || controlledEnt->IsSpectator()) {
        // The bot should respawn
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        return;
    }

    // Initialize command state
    m_botCmd.buttons |= BUTTON_RUN;  // Default to running (may be overridden in combat)
    m_botCmd.upmove = 0;

    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = controlledEnt->viewheight;
    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;

    // Validate bot state and recover from invalid conditions
    ValidateAndRecoverState();

    UpdateStates();
    if (m_pCurrentState)
    {
        m_pCurrentState->Think();
        
        // FINAL SAFETY: Ensure non-combat states can't be overridden by combat stance logic
        if (!dynamic_cast<AttackBotState*>(m_pCurrentState)) {
            m_bIsCrouching = false;
            m_botCmd.upmove = 0;  // Normal movement (physics flags do the real work)
            // FORCE clear actual player crouch flags!
            if (controlledEnt && controlledEnt->client) {
                controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
            }
        }
    }

    movement.MoveThink(m_botCmd, m_fSkill);
    rotation.TurnThink(m_botCmd, m_botEyes, m_fSkill);
    CheckUse();

    CheckValidWeapon();
}

/*
====================
ValidateAndRecoverState

Validates the bot's state and recovers from invalid conditions
====================
*/
void BotController::ValidateAndRecoverState()
{
    // Ensure we have a valid current state
    if (!m_pCurrentState) {
        gi.DPrintf("%s: No current state, defaulting to idle\n", controlledEnt->client->pers.netname);
        m_pCurrentState = m_pIdleState;
        if (m_pCurrentState) {
            m_pCurrentState->Enter();
        }
    }

    // Validate enemy reference
    if (m_pEnemy && !IsValidEnemy(m_pEnemy)) {
        // gi.DPrintf("%s: Invalid enemy reference, clearing\n", controlledEnt->client->pers.netname);
        ClearEnemy();
    }

    // Reset any stuck states
    if (m_iAttackTime && level.inttime > m_iAttackTime + 10000) { // 10 seconds max attack time
        gi.DPrintf("%s: Attack state stuck, resetting\n", controlledEnt->client->pers.netname);
        ClearEnemy();
    }

    if (m_iCuriousTime && level.inttime > m_iCuriousTime + 30000) { // 30 seconds max curious time
        gi.DPrintf("%s: Curious state stuck, resetting\n", controlledEnt->client->pers.netname);
        m_iCuriousTime = 0;
        m_vNewCuriousPos = vec_zero;
    }
}

void BotController::CheckUse(void)
{
    Vector  dir;
    Vector  start;
    Vector  end;
    trace_t trace;

    if (controlledEnt->GetLadder()) {
        return;
    }

    controlledEnt->angles.AngleVectorsLeft(&dir);

    start = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight);
    end   = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight) + dir * 64;

    trace = G_Trace(
        start, vec_zero, vec_zero, end, controlledEnt, MASK_USABLE | MASK_LADDER, false, "BotController::CheckUse"
    );

    if (!trace.ent || trace.ent->entity == world) {
        m_botCmd.buttons &= ~BUTTON_USE;
        return;
    }

    if (trace.ent->entity->IsSubclassOfDoor()) {
        Door *door = static_cast<Door *>(trace.ent->entity);
        if (door->isOpen()) {
            // Don't use an open door
            m_botCmd.buttons &= ~BUTTON_USE;
            return;
        }
    } else if (!trace.ent->entity->isSubclassOf(FuncLadder)) {
        m_botCmd.buttons &= ~BUTTON_USE;
        return;
    }

    //
    // Toggle the use button
    //
    m_botCmd.buttons ^= BUTTON_USE;

#if 0
    Vector  forward;
    Vector  start, end;

    AngleVectors(controlledEnt->GetViewAngles(), forward, NULL, NULL);

    start = (controlledEnt->m_vViewPos - forward * 12.0f);
    end   = (controlledEnt->m_vViewPos + forward * 128.0f);

    trace = G_Trace(start, vec_zero, vec_zero, end, controlledEnt, MASK_LADDER, qfalse, "checkladder");
    if (trace.ent->entity && trace.ent->entity->isSubclassOf(FuncLadder)) {
        return;
    }

    m_botCmd.buttons ^= BUTTON_USE;
#endif
}

bool BotController::CheckWindows(void)
{
    trace_t trace;
    Vector  start, end;
    Vector  dir;

    controlledEnt->angles.AngleVectorsLeft(&dir);
    start = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight);
    end   = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight) + dir * 64;

    trace = G_Trace(start, vec_zero, vec_zero, end, controlledEnt, MASK_PLAYERSOLID, false, "BotController::CheckUse");

    if (trace.fraction != 1 && trace.ent) {
        if (trace.ent->entity->isSubclassOf(WindowObject)) {
            return true;
        }
    }

    return false;
}

void BotController::CheckValidWeapon()
{
    if (!controlledEnt) {
        return;
    }

    Weapon *weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        // If holstered, use the best weapon available
        UseWeaponWithAmmo();
    } else if (!weapon->HasAmmo(FIRE_PRIMARY) && !controlledEnt->GetNewActiveWeapon()) {
        // In case the current weapon has no ammo, use the best available weapon
        UseWeaponWithAmmo();
    }
}

void BotController::SendCommand(const char *text)
{
    // Input validation
    if (!text || !*text) {
        gi.DPrintf("BotController::SendCommand: Invalid command text\n");
        return;
    }

    if (!controlledEnt) {
        gi.DPrintf("BotController::SendCommand: No controlled entity\n");
        return;
    }

    char        *buffer;
    char        *data;
    size_t       len;
    ConsoleEvent ev;

    len = strlen(text) + 1;

    buffer = (char *)gi.Malloc(len);
    if (!buffer) {
        gi.DPrintf("BotController::SendCommand: Failed to allocate memory\n");
        return;
    }

    data = buffer;
    Q_strncpyz(data, text, len);

    const char *com_token = COM_Parse(&data);

    if (!com_token) {
        return;
    }

    controlledEnt->m_lastcommand = com_token;

    if (!Event::GetEvent(com_token)) {
        return;
    }

    ev = ConsoleEvent(com_token);

    if (!(ev.GetEventFlags(ev.eventnum) & EV_CONSOLE)) {
        gi.Free(buffer);
        return;
    }

    ev.SetConsoleEdict(controlledEnt->edict);

    while (1) {
        com_token = COM_Parse(&data);

        if (!com_token || !*com_token) {
            break;
        }

        ev.AddString(com_token);
    }

    gi.Free(buffer);

    try {
        controlledEnt->ProcessEvent(ev);
    } catch (ScriptException& exc) {
        gi.DPrintf("*** Bot Command Exception *** %s\n", exc.string.c_str());
    }
}

/*
====================
AimAtAimNode

Make the bot face toward the current path
====================
*/
void BotController::AimAtAimNode(void)
{
    Vector goal;

    if (!movement.IsMoving()) {
        return;
    }

    //goal = movement.GetCurrentGoal();
    //if (goal != controlledEnt->origin) {
    //    rotation.AimAt(goal);
    //}

    if (controlledEnt->GetLadder()) {
        Vector vAngles = movement.GetCurrentPathDirection().toAngles();
        vAngles.x      = Q_clamp_float(vAngles.x, -80, 80);

        rotation.SetTargetAngles(vAngles);
        return;
    } else {
        Vector targetAngles;
        targetAngles   = movement.GetCurrentPathDirection().toAngles();
        targetAngles.x = 0;
        rotation.SetTargetAngles(targetAngles);
    }
}

/*
====================
CheckReload

Make the bot reload if necessary
====================
*/
void BotController::CheckReload(void)
{
    Weapon *weap = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (!weap) {
        return;
    }

    // Don't reload if weapon doesn't need it
    if (!weap->CheckReload(FIRE_PRIMARY)) {
        return;
    }

    // Skill-based reload behavior: lower skill bots are more likely to "forget" to reload
    // and have worse timing for when to reload
    const float forgetChance = 0.5f - (m_fSkill * 0.4f);  // 50% forget chance at skill 0, 10% at skill 1
    if (G_Random(1.0f) < forgetChance) {
        return;
    }

    // Lower skill bots wait longer before considering a reload
    const int skillBasedDelay = 1000 + (int)((1.0f - m_fSkill) * 2000);  // 1-3 second delay based on skill
    if (level.inttime < m_iLastFireTime + skillBasedDelay) {
        // Don't reload while recently attacking or if skill dictates waiting longer
        return;
    }

    // All conditions met - initiate reload
    SendCommand("reload");
}

/*
====================
NoticeEvent

Warn the bot of an event
====================
*/
void BotController::NoticeEvent(Vector vPos, int iType, Entity *pEnt, float fDistanceSquared, float fRadiusSquared)
{
    Sentient *pSentOwner;
    float     fRangeFactor;
    Vector    delta1, delta2;

    if (m_iCuriousTime) {
        delta1 = vPos - controlledEnt->origin;
        delta2 = m_vNewCuriousPos - controlledEnt->origin;
        if (delta1.lengthSquared() < delta2.lengthSquared()) {
            return;
        }
    }

    fRangeFactor = 1.0 - (fDistanceSquared / fRadiusSquared);

    if (fRangeFactor < random()) {
        return;
    }

    if (pEnt->IsSubclassOfSentient()) {
        pSentOwner = static_cast<Sentient *>(pEnt);
    } else if (pEnt->IsSubclassOfVehicleTurretGun()) {
        VehicleTurretGun *pVTG = static_cast<VehicleTurretGun *>(pEnt);
        pSentOwner             = pVTG->GetSentientOwner();
    } else if (pEnt->IsSubclassOfItem()) {
        Item *pItem = static_cast<Item *>(pEnt);
        pSentOwner  = pItem->GetOwner();
    } else if (pEnt->IsSubclassOfProjectile()) {
        Projectile *pProj = static_cast<Projectile *>(pEnt);
        pSentOwner        = pProj->GetOwner();
    } else {
        pSentOwner = NULL;
    }

    if (pSentOwner) {
        if (pSentOwner == controlledEnt) {
            // Ignore self
            return;
        }

        if ((pSentOwner->flags & FL_NOTARGET) || pSentOwner->getSolidType() == SOLID_NOT) {
            return;
        }

        // Ignore teammates
        if (pSentOwner->IsSubclassOfPlayer()) {
            Player *p = static_cast<Player *>(pSentOwner);

            if (g_gametype->integer >= GT_TEAM && p->GetTeam() == controlledEnt->GetTeam()) {
                return;
            }
        }
    }

    switch (iType) {
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
        // Minor events just make bots curious
        m_iCuriousTime   = level.inttime + 10000;  // Reduced from 20 seconds
        m_vNewCuriousPos = vPos;
        break;
        
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_WEAPON_IMPACT:
    case AI_EVENT_EXPLOSION:
        // Combat events - if we have a valid enemy owner, immediately target them
        if (pSentOwner && IsValidEnemy(pSentOwner)) {
            m_pEnemy = pSentOwner;
            m_vLastEnemyPos = pSentOwner->origin;
            m_iLastSeenTime = level.inttime;
            m_iAttackTime = level.inttime + 5000;  // Stay in attack mode for 5 seconds
            m_iEnemyEyesTag = -1;  // Reset eye tracking
            
            // Clear any current movement to focus on combat
            movement.ClearMove();
            
            // gi.DPrintf("%s: Noticed combat event from %s, engaging!\n", 
            //           controlledEnt->client->pers.netname,
            //           pSentOwner->IsSubclassOfPlayer() ? 
            //           static_cast<Player*>(pSentOwner)->client->pers.netname : "enemy");
        } else {
            // No valid enemy owner, just become curious
            m_iCuriousTime   = level.inttime + 15000;
            m_vNewCuriousPos = vPos;
        }
        break;
        
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
    case AI_EVENT_FOOTSTEP:
    case AI_EVENT_GRENADE:
    default:
        // Other events make bots curious for investigation
        m_iCuriousTime   = level.inttime + 15000;
        m_vNewCuriousPos = vPos;
        break;
    }
}

/*
====================
ClearEnemy

Clear the bot's enemy and reset combat state
====================
*/
void BotController::ClearEnemy(void)
{
    // Clear enemy tracking data
    m_iAttackTime   = 0;
    m_pEnemy        = nullptr;
    m_iEnemyEyesTag = -1;
    m_vOldEnemyPos  = vec_zero;
    m_vLastEnemyPos = vec_zero;
    
    // Reset combat movement behavior
    m_iStrafeDirection = 0;
    m_iStrafeChangeTime = 0;
    m_iLastStrafeTime = 0;

    // Reset combat stance to normal patrol behavior
    m_bIsCrouching = false;
    m_bWantsToRun = true;
    m_iCrouchTime = 0;
    m_iStandTime = 0;
    m_iLastStanceChangeTime = 0;
    
    // Clear combat-related command state
    m_botCmd.upmove = 0;
    m_botCmd.buttons |= BUTTON_RUN;
    m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
}

/*
====================
Bot states
--------------------
____________________
--------------------
____________________
--------------------
____________________
--------------------
____________________
====================
*/











static Vector bot_origin;

static int sentients_compare(const void *elem1, const void *elem2)
{
    Entity *e1, *e2;
    float   delta[3];
    float   d1, d2;

    e1 = *(Entity **)elem1;
    e2 = *(Entity **)elem2;

    VectorSubtract(bot_origin, e1->origin, delta);
    d1 = VectorLengthSquared(delta);

    VectorSubtract(bot_origin, e2->origin, delta);
    d2 = VectorLengthSquared(delta);

    if (d2 <= d1) {
        return d1 > d2;
    } else {
        return -1;
    }
}

bool BotController::IsValidEnemy(Sentient *sent) const
{
    if (!sent) {
        return false;
    }

    if (sent == controlledEnt) {
        return false;
    }

    if (sent->hidden() || (sent->flags & FL_NOTARGET)) {
        // Ignore hidden / non-target enemies
        return false;
    }

    if (sent->IsDead()) {
        // Ignore dead enemies
        return false;
    }

    if (sent->getSolidType() == SOLID_NOT) {
        // Ignore non-solid, like spectators
        return false;
    }

    // Team validation - this was the main issue
    if (sent->IsSubclassOfPlayer()) {
        Player *player = static_cast<Player *>(sent);

        // In team games, don't attack teammates
        if (g_gametype->integer >= GT_TEAM && player->GetTeam() == controlledEnt->GetTeam()) {
            return false;
        }
        
        // Debug output to verify enemy validation
        //gi.DPrintf("%s: Validating player %s - GameType: %d, MyTeam: %d, TheirTeam: %d - VALID\n",
        //          controlledEnt->client->pers.netname,
        //          player->client->pers.netname,
        //          g_gametype->integer,
        //          controlledEnt->GetTeam(),
        //          player->GetTeam());
        
        // In non-team games (FFA), all other players are valid enemies
        // In team games, players on different teams are valid enemies
        return true;
    } else {
        // For non-player sentients (NPCs), check team alignment
        if (sent->m_Team == controlledEnt->m_Team) {
            return false;
        }
        return true;
    }
}

bool BotController::CheckCondition_Attack(void)
{
    // Performance optimization - don't scan all sentients every frame
    const int currentTime = level.inttime;
    const int scanInterval = 250; // Only scan for new enemies every 250ms
    
    // Always check current enemy first if we have one
    if (m_pEnemy && IsValidEnemy(m_pEnemy)) {
        const float maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 1.2f);
        // Use wider angle for current enemy to maintain engagement
        if (controlledEnt->CanSee(m_pEnemy, 90, maxDistance, false)) {  // Reduced from 140 to 90 degrees
            // Current enemy is still valid and visible - keep tracking
            m_vLastEnemyPos = m_pEnemy->origin;
            m_iLastSeenTime = currentTime;
            m_iAttackTime = currentTime + 1000;
            return true;
        }
    }

    // Only do expensive enemy scanning periodically
    if (currentTime < m_iLastFullUpdateTime + scanInterval) {
        // Skip expensive scan, but check if attack time has expired
        if (currentTime > m_iAttackTime) {
            if (m_iAttackTime) {
                movement.ClearMove();
                m_iAttackTime = 0;
                ClearEnemy();
            }
            return false;
        }
        return m_iAttackTime > 0;
    }

    // Time for full enemy scan
    m_iLastFullUpdateTime = currentTime;
    
    Container<Sentient *> sents = SentientList;
    Sentient* bestEnemy = nullptr;
    float bestDistanceSquared = 999999.0f;
    float maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 1.2f);

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);

    // Search for new enemies
    for (int i = 1; i <= sents.NumObjects(); i++) {
        Sentient *sent = sents.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        // Check if this enemy is visible
        if (controlledEnt->CanSee(sent, 90, maxDistance, false)) {  // Reduced from 140 to 90 degrees
            float distanceSquared = (sent->origin - controlledEnt->origin).lengthSquared();
            
            // Prefer closer enemies, or if same enemy as before, prefer to keep it
            if (sent == m_pEnemy || distanceSquared < bestDistanceSquared) {
                bestEnemy = sent;
                bestDistanceSquared = distanceSquared;
            }
        }
    }

    // Update enemy tracking
    if (bestEnemy) {
        if (m_pEnemy != bestEnemy) {
            m_iEnemyEyesTag = -1; // Reset eye tracking for new enemy
            
            // Debug: Log enemy changes
            // gi.DPrintf("%s: New enemy detected: %s\n", 
            //           controlledEnt->client->pers.netname,
            //           bestEnemy->IsSubclassOfPlayer() ? 
            //           static_cast<Player*>(bestEnemy)->client->pers.netname : "NPC");
        }
        
        if (!m_pEnemy) {
            m_iLastUnseenTime = currentTime; // Transitioning from no enemy to enemy
        }

        m_pEnemy = bestEnemy;
        m_vLastEnemyPos = m_pEnemy->origin;
        m_iLastSeenTime = currentTime;
        m_iAttackTime = currentTime + 1000;
        return true;
    }

    // No enemy found - check if attack time has expired
    if (currentTime > m_iAttackTime) {
        if (m_iAttackTime) {
            movement.ClearMove();
            m_iAttackTime = 0;
            ClearEnemy(); // Clean up enemy state when giving up attack
        }
        return false;
    }

    // Still in attack time window but no visible enemy - keep attack state
    return true;
}

bool BotController::CheckCondition_Curious(void)
{
    // Don't be curious while in active combat
    if (m_iAttackTime) {
        m_iCuriousTime = 0;
        return false;
    }

    // Check if curiosity time has expired
    if (level.inttime > m_iCuriousTime) {
        if (m_iCuriousTime) {
            // Curiosity period ended - clear movement and reset
            movement.ClearMove();
            m_iCuriousTime = 0;
            m_vNewCuriousPos = vec_zero;
        }
        return false;
    }

    // Still curious - validate that we have a valid position to investigate
    if (m_vNewCuriousPos == vec_zero) {
        m_iCuriousTime = 0;
        return false;
    }

    return true;
}

/*
====================
AimAtEnemy

Make the bot aim at the enemy
====================
*/
void BotController::AimAtEnemy(bool bCanSee, float fDistanceSquared, bool bFiring)
{
    if (bCanSee || level.inttime < m_iAttackStopAimTime) {
        Vector        vTarget;
        orientation_t eyes_or;

        if (m_iEnemyEyesTag == -1) {
            // Cache the tag
            m_iEnemyEyesTag = gi.Tag_NumForName(m_pEnemy->edict->tiki, "eyes bone");
        }

        if (m_iEnemyEyesTag != -1) {
            // Use the enemy's eyes bone for headshots (higher skill bots prefer this)
            m_pEnemy->GetTag(m_iEnemyEyesTag, &eyes_or);
            vTarget = eyes_or.origin;
        } else {
            // Fallback to origin
            vTarget = m_pEnemy->origin;
        }

        if (level.inttime >= m_iLastAimTime + 100) {
            // Enhanced skill-based accuracy system with body part targeting
            const float baseInaccuracy = 1.5f - (m_fSkill * 1.0f);  // Range from 1.5 (skill 0) to 0.5 (skill 1)
            
            // Distance-based accuracy degradation
            const float distance = sqrt(fDistanceSquared);
            const float distanceFactor = 1.0f + (distance / 512.0f);  // Gets worse with distance
            
            // Movement-based accuracy penalty
            const float velocity = controlledEnt->velocity.length();
            const float movementPenalty = 1.0f + (velocity / 200.0f);  // Penalty for moving
            
            // Stance-based accuracy modifier
            float stanceModifier = 1.0f;
            if (m_bIsCrouching) {
                stanceModifier = 0.4f;  // Much better accuracy when crouching
            } else if (!m_bWantsToRun) {
                stanceModifier = 0.7f;  // Better accuracy when not running
            }
            
            // Weapon spread factor influence
            Weapon *weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
            float weaponSpreadFactor = 1.0f;
            if (weapon) {
                weaponSpreadFactor = 1.0f + weapon->GetSpreadFactor(FIRE_PRIMARY) * 2.0f;
            }
            
            // Burst fire penalty - accuracy degrades during continuous fire
            float burstPenalty = 1.0f;
            if (m_iContinuousFireTime > 0) {
                burstPenalty = 1.0f + (m_iContinuousFireTime / 1000.0f) * 0.5f;  // +50% penalty per second of continuous fire
            }
            
            // Target movement penalty - harder to hit moving targets
            float targetMovementPenalty = 1.0f;
            if (m_pEnemy) {
                const float enemyVelocity = m_pEnemy->velocity.length();
                targetMovementPenalty = 1.0f + (enemyVelocity / 300.0f);  // Penalty for moving targets
            }
            
            // Final accuracy calculation
            const float finalInaccuracy = baseInaccuracy * distanceFactor * movementPenalty * stanceModifier * weaponSpreadFactor * burstPenalty * targetMovementPenalty;
            
            // Body part targeting based on skill and accuracy
            // High skill bots aim for head more often, low skill bots aim for center mass
            const float headChance = m_fSkill * 0.4f;  // 0-40% chance based on skill
            const bool aimForHead = (G_Random() < headChance);
            
            if (m_iEnemyEyesTag != -1) {
                // We have eyes bone - can target head or body
                if (aimForHead) {
                    // Aiming at head area (eyes bone) - use original old code logic for head targeting
                    const float headSpread = finalInaccuracy * 2.0f;  // Smaller spread for head shots
                    m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.3f);
                    m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.3f);
                    m_vAimOffset[2] = -G_Random(m_pEnemy->maxs.z * 0.3f) + G_CRandom(headSpread);
                } else {
                    // Aiming at body/center mass - move down from head
                    const float bodySpread = finalInaccuracy * 3.0f;
                    m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5f);
                    m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5f);
                    m_vAimOffset[2] = -G_Random(m_pEnemy->maxs.z * 0.7f) + G_CRandom(bodySpread);  // Aim lower for body
                }
            } else {
                // No eyes bone - use origin-based targeting from old code
                const float bodySpread = finalInaccuracy * 4.0f;
                m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5f);
                m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5f);
                m_vAimOffset[2] = 16 + G_Random(m_pEnemy->viewheight - 16) + G_CRandom(bodySpread);  // Original old code logic
            }
            
            // Add some aim drift during sustained combat for realism
            if (bFiring && m_iContinuousFireTime > 500) {
                const float driftAmount = (1.0f - m_fSkill) * 8.0f;  // More drift for lower skill
                m_vAimOffset[0] += G_CRandom(driftAmount);
                m_vAimOffset[1] += G_CRandom(driftAmount);
            }

            m_iLastAimTime = level.inttime;
        }

        rotation.AimAt(vTarget + m_vAimOffset);
    } else {
        AimAtAimNode();
    }
}

/*
====================
PerformAttack

Make the bot attack the enemy
====================
*/
bool BotController::PerformAttack(bool bCanSee, bool bCanAttack, float fDistanceSquared, bool& bMelee, bool& bNoMove, float& fMinDistanceSquared, Weapon* pWeap)
{
    bool bFiring = false;

    if (bCanAttack) {
        const int fireDelay                    = pWeap->FireDelay(FIRE_PRIMARY) * 1000;
        float     fPrimaryBulletRange          = pWeap->GetBulletRange(FIRE_PRIMARY);  // REMOVED: / 1.25f division
        float     fPrimaryBulletRangeSquared   = fPrimaryBulletRange * fPrimaryBulletRange;
        float     fSecondaryBulletRange        = pWeap->GetBulletRange(FIRE_SECONDARY);
        float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;
        float     fSpreadFactor                = pWeap->GetSpreadFactor(FIRE_PRIMARY);

        // Ensure minimum firing range - some weapons may have unreasonably short ranges configured
        const float minFiringRange = 2048.0f;  // Minimum 2048 units firing range
        if (fPrimaryBulletRange < minFiringRange) {
            fPrimaryBulletRange = minFiringRange;
            fPrimaryBulletRangeSquared = fPrimaryBulletRange * fPrimaryBulletRange;
        }

        const int maxContinousFireTime = fireDelay + 500 + G_Random(1500.0f * m_fSkill);
        const int maxBurstTime         = fireDelay + 100 + G_Random(500.0f * (1.0f - m_fSkill));

        //
        // check the fire movement speed if the weapon has a max fire movement
        //
        if (pWeap->GetMaxFireMovement() < 1 && pWeap->HasAmmoInClip(FIRE_PRIMARY)) {
            float length;

            length = controlledEnt->velocity.length();
            if ((length / sv_runspeed->value) > (pWeap->GetMaxFireMovementMult())) {
                bNoMove = true;
                movement.ClearMove();
            }
        }

        float fMinDistance = fPrimaryBulletRange;

        if (fMinDistance > 256) {
            fMinDistance = 256;
        }

        fMinDistanceSquared = fMinDistance * fMinDistance;

        if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0
            && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
            m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
            controlledEnt->ZoomOff();
        } else if (fDistanceSquared > fPrimaryBulletRangeSquared) {
            m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
            controlledEnt->ZoomOff();
        } else {
            //
            // Attacking
            //

            if (pWeap->IsSemiAuto()) {
                if (controlledEnt->client->ps.iViewModelAnim != VM_ANIM_IDLE
                    && (controlledEnt->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                        || controlledEnt->client->ps.iViewModelAnim > VM_ANIM_IDLE_2)) {
                    m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                    controlledEnt->ZoomOff();
                } else if (fSpreadFactor < 0.25) {
                    bFiring = true;
                    m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
                    if (pWeap->GetZoom()) {
                        if (!controlledEnt->IsZoomed()) {
                            m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                        } else {
                            m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                        }
                    }
                } else {
                    bNoMove = true;
                    movement.ClearMove();
                }
            } else {
                bFiring = true;
                m_botCmd.buttons |= BUTTON_ATTACKLEFT;
            }
        }

        //
        // Burst
        //

        if (m_iLastBurstTime) {
            if (level.inttime > m_iLastBurstTime + maxBurstTime) {
                m_iLastBurstTime      = 0;
                m_iContinuousFireTime = 0;
            } else {
                m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            }
        } else {
            if (bFiring) {
                m_iContinuousFireTime += level.intframetime;
            } else {
                m_iContinuousFireTime = 0;
            }

            if (!m_iLastBurstTime && m_iContinuousFireTime > maxContinousFireTime) {
                m_iLastBurstTime      = level.inttime;
                m_iContinuousFireTime = 0;
            }
        }

        m_iLastFireTime = level.inttime;

        if (pWeap->GetFireType(FIRE_SECONDARY) == FT_MELEE) {
            if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                bMelee = true;
            } else if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                bMelee = true;
            }
        }

        if (bMelee) {
            m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;

            if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                m_botCmd.buttons ^= BUTTON_ATTACKRIGHT;
            } else {
                m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
            }
        }

        m_iAttackTime        = level.inttime + 1000;
        m_iAttackStopAimTime = level.inttime + 3000;
        m_iLastSeenTime      = level.inttime;
        m_vLastEnemyPos      = m_pEnemy->origin;
    }

    return bFiring;
}

/*
====================
PerformCombatMovement

Make the bot move during combat
====================
*/
bool BotController::PerformCombatMovement(bool bCanSee, bool bMelee, bool bNoMove, bool bFiring, float fMinDistanceSquared, float fEnemyDistanceSquared)
{
    if (bNoMove) {
        // Weapon requires standing still for accuracy - clear movement
        movement.ClearMove();
        return false;
    }

    // Update combat stance ONLY when in combat state - prevent crouch persistence in other states
    if ((bCanSee || level.inttime < m_iAttackStopAimTime) && dynamic_cast<AttackBotState*>(m_pCurrentState)) {
        UpdateCombatStance();
    } else {
        // Reset combat stance when no longer in combat OR not in attack state
        m_bIsCrouching = false;
        m_bWantsToRun = true;
        m_botCmd.upmove = 0;  // Normal movement (physics flags do the real work)
        m_botCmd.buttons |= BUTTON_RUN;
        // FORCE clear actual player crouch flags!
        if (controlledEnt && controlledEnt->client) {
            controlledEnt->client->ps.pm_flags &= ~(PMF_DUCKED | PMF_VIEW_PRONE | PMF_VIEW_DUCK_RUN);
        }
    }

    // Determine if bot should stand still for accuracy - INCREASED likelihood
    bool bShouldStandStill = false;
    
    // Stand still in more situations for better accuracy:
    // 1. Bot is firing at medium/long range (all skill levels benefit from this)
    // 2. Bot is crouching (should always stand still when crouched for best accuracy)
    // 3. High skill bot choosing precision over mobility
    // 4. Any bot firing at very long range
    if (bCanSee && bFiring && (
        (fEnemyDistanceSquared >= Square(192)) ||  // Stand still at medium+ range when firing
        (m_bIsCrouching) ||  // Always stand still when crouched
        (!m_bWantsToRun && m_fSkill > 0.5f) ||  // Medium+ skill bots choosing accuracy
        (fEnemyDistanceSquared >= Square(384) && G_Random(1.0f) < 0.6f))) {  // 60% chance at long range
        bShouldStandStill = true;
    }
    
    // Additional chance for high skill bots to stand still for precision shots
    if (bCanSee && bFiring && m_fSkill > 0.7f && G_Random(1.0f) < 0.4f) {
        bShouldStandStill = true;  // 40% chance for high skill bots to prioritize accuracy
    }

    // Clear movement for accuracy only in very specific cases
    if (bShouldStandStill) {
        movement.ClearMove();
    }

    // Enhanced strafing behavior - allow strafing in most combat situations
    if (bCanSee && !bMelee && !bShouldStandStill) {
        // Ensure bots run when moving during combat (not walking)
        if (!m_bIsCrouching) {
            m_botCmd.buttons |= BUTTON_RUN;
        }
        
        // Strafe when:
        // 1. Currently firing OR recently fired (within last 500ms)
        // 2. Enemy is close (always strafe when close)
        // 3. Bot wants to run (mobile combat style)
        bool shouldStrafe = bFiring || 
                           (level.inttime - m_iLastFireTime < 500) ||
                           (fEnemyDistanceSquared < Square(256)) ||
                           (m_bWantsToRun);
                           
        if (shouldStrafe) {
            PerformCombatStrafing();
        }
    }

    if (!bShouldStandStill && 
        ((!movement.MoveToBestAttractivePoint(5) && !movement.IsMoving())
        || (m_vOldEnemyPos != m_vLastEnemyPos && !movement.MoveDone()) || fEnemyDistanceSquared < fMinDistanceSquared)) {
        
        // Ensure bots run when actively moving during combat
        if (!m_bIsCrouching) {
            m_botCmd.buttons |= BUTTON_RUN;
        }
        
        if (!bMelee || !bCanSee) {
            if (fEnemyDistanceSquared < fMinDistanceSquared) {
                Vector vDir = controlledEnt->origin - m_vLastEnemyPos;
                VectorNormalizeFast(vDir);

                movement.AvoidPath(m_vLastEnemyPos, sqrt(fMinDistanceSquared), Vector(controlledEnt->orientation[1]) * 512);
            } else {
                movement.MoveTo(m_vLastEnemyPos);
            }

            if (!bCanSee && movement.MoveDone()) {
                // Lost track of the enemy
                ClearEnemy();
                return true;
            }
        } else {
            movement.MoveTo(m_vLastEnemyPos);
        }
    }

    if (movement.IsMoving()) {
        m_iAttackTime = level.inttime + 1000;
    }

    return false;
}

/*
====================
CheckReactionTime

Check the bot's reaction time
====================
*/
bool BotController::CheckReactionTime(float fDistanceSquared)
{
    if (m_iLastUnseenTime) {
        // Improved skill-based reaction time
        // Lower skill = higher multiplier and more variance
        const float skillFactor = 3.0f - (m_fSkill * 2.0f);  // Range from 3.0 (skill 0) to 1.0 (skill 1)

        // Base reaction time varies by distance - closer enemies are noticed faster
        const float baseReactionTime = Q_min(1000 * Q_min(1, fDistanceSquared / Square(1536)), 1000);
        
        // Additional random variance based on skill - low skill bots are more inconsistent
        const float skillVariance = (1.0f - m_fSkill) * 600.0f;  // 0-600ms additional variance
        const float randomVariance = G_Random(skillVariance);
        
        // Final reaction time: base + random variance, all modified by skill
        const float totalReactionTime = (baseReactionTime + 300 + randomVariance) * skillFactor;
        
        if (level.inttime <= m_iLastUnseenTime + totalReactionTime) {
            return false;
        } else {
            m_iLastUnseenTime = 0;
        }
    }

    return true;
}







Weapon *BotController::FindWeaponWithAmmo()
{
    Weapon               *next;
    int                   n;
    int                   j;
    int                   bestrank;
    Weapon               *bestweapon;
    const Container<int>& inventory = controlledEnt->getInventory();

    n = inventory.NumObjects();

    // Search until we find the best weapon with ammo
    bestweapon = NULL;
    bestrank   = -999999;

    for (j = 1; j <= n; j++) {
        next = (Weapon *)G_GetEntity(inventory.ObjectAt(j));

        assert(next);
        if (!next->IsSubclassOfWeapon() || next->IsSubclassOfInventoryItem()) {
            continue;
        }

        if (next->GetWeaponClass() & WEAPON_CLASS_THROWABLE) {
            continue;
        }

        if (next->GetRank() < bestrank) {
            continue;
        }

        if (!next->HasAmmo(FIRE_PRIMARY)) {
            continue;
        }

        bestweapon = (Weapon *)next;
        bestrank   = bestweapon->GetRank();
    }

    return bestweapon;
}

Weapon *BotController::FindMeleeWeapon()
{
    Weapon               *next;
    int                   n;
    int                   j;
    int                   bestrank;
    Weapon               *bestweapon;
    const Container<int>& inventory = controlledEnt->getInventory();

    n = inventory.NumObjects();

    // Search until we find the best weapon with ammo
    bestweapon = NULL;
    bestrank   = -999999;

    for (j = 1; j <= n; j++) {
        next = (Weapon *)G_GetEntity(inventory.ObjectAt(j));

        assert(next);
        if (!next->IsSubclassOfWeapon() || next->IsSubclassOfInventoryItem()) {
            continue;
        }

        if (next->GetRank() < bestrank) {
            continue;
        }

        if (next->GetFireType(FIRE_SECONDARY) != FT_MELEE) {
            continue;
        }

        bestweapon = (Weapon *)next;
        bestrank   = bestweapon->GetRank();
    }

    return bestweapon;
}

void BotController::UseWeaponWithAmmo()
{
    // Skill-based weapon switching - lower skill bots are slower to react to ammo changes
    static int lastWeaponCheckTime = 0;
    const int skillBasedCheckDelay = 500 + (int)((1.0f - m_fSkill) * 1500);  // 0.5-2 second delay based on skill
    
    if (level.inttime < lastWeaponCheckTime + skillBasedCheckDelay) {
        return;  // Don't check for weapon changes too frequently if low skill
    }
    lastWeaponCheckTime = level.inttime;

    Weapon *bestWeapon = FindWeaponWithAmmo();
    if (!bestWeapon) {
        //
        // If there is no weapon with ammo, fallback to a weapon that can melee
        //
        bestWeapon = FindMeleeWeapon();
    }

    if (!bestWeapon || bestWeapon == controlledEnt->GetActiveWeapon(WEAPON_MAIN)) {
        return;
    }

    controlledEnt->useWeapon(bestWeapon, WEAPON_MAIN);
}

void BotController::Spawned(void)
{
    ClearEnemy();
    m_iCuriousTime   = 0;
    m_botCmd.buttons = 0;
}

void BotController::Think()
{
    usercmd_t  ucmd;
    usereyes_t eyeinfo;

    UpdateBotStates();
    GetUsercmd(&ucmd);
    GetEyeInfo(&eyeinfo);

    G_ClientThink(controlledEnt->edict, &ucmd, &eyeinfo);
}

void BotController::Killed(const Event& ev)
{
    Entity *attacker;

    // send the respawn buttons
    if (!(m_botCmd.buttons & BUTTON_ATTACKLEFT)) {
        m_botCmd.buttons |= BUTTON_ATTACKLEFT;
    } else {
        m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
    }

    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = 0;
    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;

    attacker = ev.GetEntity(1);

    if (attacker && rand() % 2 == 0) {
        // 1/2 chance to go back to the attacker position
        m_vLastDeathPos = attacker->origin;
    } else {
        m_vLastDeathPos = vec_zero;
    }

    // Choose a new random primary weapon
    Event event(EV_Player_PrimaryDMWeapon);
    event.AddString("auto");

    controlledEnt->ProcessEvent(event);

    //
    // This is useful to change nationality in Spearhead and Breakthrough
    // this allows the AI to use more weapons
    //
    //Info_SetValueForKey(controlledEnt->client->pers.userinfo, "dm_playermodel", G_GetRandomAlliedPlayerModel());
    //Info_SetValueForKey(controlledEnt->client->pers.userinfo, "dm_playergermanmodel", G_GetRandomGermanPlayerModel());

    G_ClientUserinfoChanged(controlledEnt->edict, controlledEnt->client->pers.userinfo);
}

void BotController::GotKill(const Event& ev)
{
    ClearEnemy();
    m_iCuriousTime = 0;
    
    if (level.inttime >= m_iNextTauntTime && (rand() % 5) == 0) {
        //
        // Randomly play a taunt
        //
        Event event("dmmessage");

        event.AddInteger(0);

        if (g_protocol >= protocol_e::PROTOCOL_MOHTA_MIN) {
            event.AddString("*5" + str(1 + (rand() % 8)));
        } else {
            event.AddString("*4" + str(1 + (rand() % 9)));
        }

        controlledEnt->ProcessEvent(event);

        m_iNextTauntTime = level.inttime + 8000;
    }
}

void BotController::EventStuffText(const str& text)
{
    SendCommand(text);
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

BotController *BotControllerManager::createController(Player *player)
{
    BotController *controller = new BotController();
    controller->setControlledEntity(player);

    controllers.AddObject(controller);

    return controller;
}

void BotControllerManager::removeController(BotController *controller)
{
    controllers.RemoveObject(controller);
    delete controller;
}

BotController *BotControllerManager::findController(Entity *ent)
{
    int i;

    for (i = 1; i <= controllers.NumObjects(); i++) {
        BotController *controller = controllers.ObjectAt(i);
        if (controller->getControlledEntity() == ent) {
            return controller;
        }
    }

    return nullptr;
}

const Container<BotController *>& BotControllerManager::getControllers() const
{
    return controllers;
}

BotControllerManager::~BotControllerManager()
{
    Cleanup();
}

void BotControllerManager::Init()
{
    BotController::Init();
}

void BotControllerManager::Cleanup()
{
    int i;

    BotController::Init();

    for (i = 1; i <= controllers.NumObjects(); i++) {
        BotController *controller = controllers.ObjectAt(i);
        delete controller;
    }

    controllers.FreeObjectList();
}

void BotControllerManager::ThinkControllers()
{
    int i;

    // Delete controllers that don't have associated player entity
    // This cannot happen unless some mods remove them
    for (i = controllers.NumObjects(); i > 0; i--) {
        BotController *controller = controllers.ObjectAt(i);
        if (!controller->getControlledEntity()) {
            gi.DPrintf(
                "Bot %d has no associated player entity. This shouldn't happen unless the entity has been removed by a "
                "script. The controller will be removed, please fix.\n",
                i
            );

            // Remove the controller, it will be recreated later to match `sv_numbots`
            delete controller;
            controllers.RemoveObjectAt(i);
        }
    }

    for (i = 1; i <= controllers.NumObjects(); i++) {
        BotController *controller = controllers.ObjectAt(i);
        controller->Think();
    }
}

/*
====================
PerformCombatStrafing

Handle skill-based strafing behavior during combat
====================
*/
void BotController::PerformCombatStrafing(void)
{
    // Enhanced skill-based strafing parameters - MORE aggressive strafing
    const float skillFactor = 0.4f + (m_fSkill * 0.6f);  // 40-100% strafing likelihood (INCREASED base)
    const int minStrafeTime = (int)(300 + (1.0f - m_fSkill) * 700);  // 0.3-1.0 seconds (FASTER changes)
    const int maxStrafeTime = (int)(800 + (1.0f - m_fSkill) * 1200); // 0.8-2.0 seconds (FASTER changes)
    
    // Allow strafing even when crouched (but with reduced intensity)
    float strafeSkillFactor = skillFactor;
    if (m_bIsCrouching) {
        strafeSkillFactor *= 0.8f;  // Only slightly reduce strafing when crouched (was 0.5f)
    }
    
    // Much more likely to strafe during combat
    if (G_Random(1.0f) > strafeSkillFactor) {
        // Still strafe occasionally even when "not strafing" for variety
        if (G_Random(1.0f) < 0.3f) {
            m_botCmd.rightmove = (int)(G_CRandom(64) * m_fSkill); // Light random movement
        } else {
            m_botCmd.rightmove = 0;
        }
        return;
    }
    
    // Check if it's time to change strafe direction (more frequent changes)
    if (level.inttime >= m_iStrafeChangeTime || m_iStrafeDirection == 0) {
        // Determine new strafe direction with enhanced variety
        const float directionChange = G_Random(1.0f);
        
        if (directionChange < 0.35f) {
            m_iStrafeDirection = -127;  // Strafe left
        } else if (directionChange < 0.7f) {
            m_iStrafeDirection = 127;   // Strafe right
        } else if (directionChange < 0.85f) {
            // Quick direction switches for high skill bots
            m_iStrafeDirection = (m_iStrafeDirection == 127) ? -127 : 127;
        } else {
            m_iStrafeDirection = 0;     // Stop strafing briefly (less often)
        }
        
        // Calculate next direction change time - faster for high skill
        const int strafeTime = minStrafeTime + G_Random(maxStrafeTime - minStrafeTime);
        m_iStrafeChangeTime = level.inttime + strafeTime;
        m_iLastStrafeTime = level.inttime;
    }
    
    // Apply strafing movement with enhanced skill-based intensity
    float intensityFactor = 0.6f + (m_fSkill * 0.4f);  // 60-100% intensity (INCREASED base)
    
    // Less intensity reduction when crouched - allow mobile crouched combat
    if (m_bIsCrouching) {
        intensityFactor *= 0.8f;  // Only slight reduction (was 0.6f)
    }
    
    m_botCmd.rightmove = (int)(m_iStrafeDirection * intensityFactor);
    
    // Allow combining movement even when crouching for dynamic combat
    // Higher skill bots use more complex movement patterns
    if (m_fSkill > 0.5f && G_Random(1.0f) < 0.3f) {
        // More frequent forward/backward movement while strafing
        if (G_Random(1.0f) < 0.7f) {
            m_botCmd.forwardmove = (int)(48 * m_fSkill);  // Move forward (INCREASED intensity)
        } else {
            m_botCmd.forwardmove = (int)(-24 * m_fSkill); // Move backward
        }
    }
    
    // Add unpredictable movement bursts for high skill bots
    if (m_fSkill > 0.7f && G_Random(1.0f) < 0.1f) {
        // Sudden burst movements
        m_botCmd.rightmove = (int)(G_CRandom(127) * 1.2f);
        m_botCmd.forwardmove = (int)(G_CRandom(64) * 0.8f);
    }
}

/*
====================
UpdateCombatStance

Handle realistic combat stance behavior - running, standing, crouching
====================
*/
void BotController::UpdateCombatStance(void)
{
    const int currentTime = level.inttime;
    
    // Skill-based stance behavior parameters - BALANCED mobility vs accuracy
    const float skillFactor = m_fSkill;
    const float crouchChance = 0.15f + (skillFactor * 0.2f);  // 15-35% chance to use tactical crouching
    const float runningChance = 0.6f - (skillFactor * 0.2f);  // 60% (low skill) to 40% (high skill) chance to always run (REDUCED for more standing)
    
    // Check if it's time to evaluate stance change
    if (currentTime >= m_iLastStanceChangeTime + 2000) {  // Check every 2 seconds (less frequent changes)
        m_iLastStanceChangeTime = currentTime;
        
        // Determine if bot should run during combat - ENHANCED accuracy decision making
        float adjustedRunningChance = runningChance;
        
        // Reduce running chance when enemy is at good firing range (encourage standing still)
        if (m_pEnemy) {
            float distanceToEnemy = (m_pEnemy->origin - controlledEnt->origin).length();
            if (distanceToEnemy >= 128.0f && distanceToEnemy <= 512.0f) {
                adjustedRunningChance *= 0.7f;  // 30% less likely to run at optimal firing range
            }
            if (distanceToEnemy > 512.0f) {
                adjustedRunningChance *= 0.5f;  // 50% less likely to run at long range (accuracy is key)
            }
        }
        
        // High skill bots are more likely to choose accuracy over mobility
        if (skillFactor > 0.6f) {
            adjustedRunningChance *= 0.8f;  // High skill bots 20% less likely to always run
        }
        
        if (G_Random(1.0f) < adjustedRunningChance) {
            // Bot prefers mobile combat
            m_bWantsToRun = true;
        } else {
            // Bot chooses accuracy - stand still for precision shots
            m_bWantsToRun = false;
        }
        
        // Determine crouching behavior based on skill and situation
        float distanceToEnemy = 0.0f;
        if (m_pEnemy) {
            distanceToEnemy = (m_pEnemy->origin - controlledEnt->origin).length();
        }
        
        // Only slightly more likely to crouch at very long distances
        float adjustedCrouchChance = crouchChance;
        if (distanceToEnemy > 512.0f) {
            adjustedCrouchChance += 0.1f;  // +10% chance at long range (REDUCED)
        }
        if (distanceToEnemy > 1024.0f) {
            adjustedCrouchChance += 0.1f;  // +20% total at very long range (REDUCED)
        }
        
        // Don't encourage crouching at close range - favor strafing instead
        if (distanceToEnemy < 256.0f) {
            adjustedCrouchChance *= 0.3f;  // Much less crouching up close
            m_bWantsToRun = true;  // Force mobile combat at close range
        }
        
        // High skill bots prefer mobile combat over crouching
        if (skillFactor > 0.7f) {
            adjustedCrouchChance *= 0.7f;  // REDUCED crouching for high skill bots
        }
        
        // Decide whether to crouch or stand - heavily favor standing/strafing
        if (G_Random(1.0f) < adjustedCrouchChance) {
            if (!m_bIsCrouching) {
                m_bIsCrouching = true;
                m_iCrouchTime = currentTime + (int)(800 + G_Random(1000)); // 0.8-1.8 seconds (MUCH shorter)
                // Don't force running to false - allow crouched strafing
            }
        } else {
            if (m_bIsCrouching) {
                m_bIsCrouching = false;
                m_iStandTime = currentTime + (int)(500 + G_Random(800)); // 0.5-1.3 seconds (shorter)
            }
        }
    }
    
    // Handle crouch timing - exit crouching quickly to return to mobile combat
    if (m_bIsCrouching && currentTime >= m_iCrouchTime) {
        m_bIsCrouching = false;
        m_iStandTime = currentTime + (int)(500 + G_Random(800)); // Force standing time after crouch
    }
    
    // Handle stand timing  
    if (!m_bIsCrouching && currentTime < m_iStandTime) {
        // Force standing for a minimum time to ensure mobile combat periods
        m_bIsCrouching = false;
    }
    
    // Apply stance to bot commands
    if (m_bIsCrouching) {
        m_botCmd.upmove = -127;  // Crouch
        m_botCmd.buttons &= ~BUTTON_RUN;  // Don't run while crouched
    } else {
        m_botCmd.upmove = 0;  // Stand normally
        // Always enable running when not crouched - individual movement logic will decide when to actually move
        m_botCmd.buttons |= BUTTON_RUN;
    }
}
