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

// We assume that we have limited access to the server-side
// and that most logic come from the playerstate_s structure

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

    m_iStrafeTime = 0;
    m_iStrafeDir  = 0;

    m_pGrenade          = NULL;
    m_iGrenadeAvoidTime = 0;

    m_iIdlePauseTime = 0;
    m_iIdleLookTime  = 0;
    m_iWalkTime      = 0;
    m_iLeanTime      = 0;
    m_iLeanDir       = 0;
    m_bIdlePausing   = false;
    m_bWalking       = false;
    m_bStandingStill = false;
    m_bCrouching     = false;
    m_bCrouchDecided = false;

    m_iAimLerpStartTime = 0;

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

BotMovement& BotController::GetMovement()
{
    return movement;
}

BotBeliefMap& BotController::GetBeliefMap()
{
    return beliefMap;
}

// Added in OPM
//  Draw debug visualization of belief zones. Each zone is drawn as a
//  colored circle at its centroid: green (low) → yellow → red (high).
//  Only drawn for the first bot to avoid visual clutter.
void BotController::DrawDebugBeliefs()
{
    if (!beliefMap.IsInitialized()) {
        return;
    }

    // Only draw for the first bot to avoid clutter
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();
    if (controllers.NumObjects() > 0 && controllers.ObjectAt(1) != this) {
        return;
    }

    const Container<BeliefZone>& zones = beliefMap.GetZones();

    for (int i = 1; i <= zones.NumObjects(); i++) {
        const BeliefZone& zone = zones.ObjectAt(i);
        if (zone.belief < 0.05f) {
            continue;
        }

        float r, g, b;
        if (zone.belief < 0.5f) {
            // Green to yellow
            r = zone.belief * 2.0f;
            g = 1.0f;
            b = 0.0f;
        } else {
            // Yellow to red
            r = 1.0f;
            g = 1.0f - (zone.belief - 0.5f) * 2.0f;
            b = 0.0f;
        }

        float  radius = 32.0f + zone.belief * 64.0f;
        Vector pos    = zone.centroid;
        G_DebugCircle((float *)pos, radius, r, g, b, zone.belief, qtrue);
    }
}

void BotController::Init(void)
{
    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        botfuncs[i].BeginState = &BotController::State_DefaultBegin;
        botfuncs[i].EndState   = &BotController::State_DefaultEnd;
    }

    InitState_Attack(&botfuncs[0]);
    InitState_Curious(&botfuncs[1]);
    InitState_Grenade(&botfuncs[2]);
    InitState_Idle(&botfuncs[3]);
    //InitState_Weapon(&botfuncs[4]);
}

void BotController::GetUsercmd(usercmd_t *ucmd)
{
    *ucmd = m_botCmd;
}

void BotController::GetEyeInfo(usereyes_t *eyeinfo)
{
    *eyeinfo = m_botEyes;
}

void BotController::UpdateBotStates(void)
{
    m_botCmd.serverTime = level.svsTime;

    if (g_bot_manualmove->integer) {
        m_botCmd.buttons     = 0;
        m_botCmd.forwardmove = m_botCmd.rightmove = m_botCmd.upmove = 0;
        return;
    }

    if (!controlledEnt->client->pers.dm_primary[0]) {
        Event *event;

        //
        // Primary weapon
        //
        event = new Event(EV_Player_PrimaryDMWeapon);
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

    //
    // Added in OPM
    //  Determine run/walk behavior based on context
    //  - Walk when idle pausing, standing still to aim, or randomly walking
    //  - Run otherwise
    //
    if (m_bIdlePausing || m_bStandingStill || m_bWalking) {
        m_botCmd.buttons &= ~BUTTON_RUN;
    } else {
        m_botCmd.buttons |= BUTTON_RUN;
    }

    //
    // Added in OPM
    //  Handle leaning during combat
    //
    m_botCmd.buttons &= ~(BUTTON_LEAN_LEFT | BUTTON_LEAN_RIGHT);
    if (m_iLeanDir < 0) {
        m_botCmd.buttons |= BUTTON_LEAN_LEFT;
    } else if (m_iLeanDir > 0) {
        m_botCmd.buttons |= BUTTON_LEAN_RIGHT;
    }

    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = controlledEnt->viewheight;
    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;

    // Added in OPM
    //  Per-frame belief map maintenance
    beliefMap.Decay(level.frametime);
    beliefMap.ClearZonesVisibleFrom(controlledEnt);

    CheckStates();

    movement.MoveThink(m_botCmd);
    rotation.TurnThink(m_botCmd, m_botEyes);
    CheckUse();

    CheckValidWeapon();

    // Added in OPM
    //  Debug visualization of belief zones
    if (g_bot_debug_beliefs->integer) {
        DrawDebugBeliefs();
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
    char        *buffer;
    char        *data;
    size_t       len;
    ConsoleEvent ev;

    len = strlen(text) + 1;

    buffer = (char *)gi.Malloc(len);
    data   = buffer;
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
    Weapon *weap;

    if (level.inttime < m_iLastFireTime + 2000) {
        // Don't reload while attacking
        return;
    }

    weap = controlledEnt->GetActiveWeapon(WEAPON_MAIN);

    if (weap && weap->CheckReload(FIRE_PRIMARY)) {
        SendCommand("reload");
    }
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
        if (delta1.lengthSquared() > delta2.lengthSquared()) {
            // Fixed in OPM
            //  Was using '<' which caused the bot to ignore closer sounds
            //  (like nearby gunfire) when already curious about a distant event.
            //  Now ignores farther sounds so closer threats take priority.
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

    // Added in OPM
    //  Feed the belief map with event data
    beliefMap.UpdateFromEvent(vPos, iType, fRangeFactor);

    switch (iType) {
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
        break;
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_WEAPON_IMPACT:
    case AI_EVENT_EXPLOSION:
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
    case AI_EVENT_FOOTSTEP:
    case AI_EVENT_GRENADE:
    default:
        m_iCuriousTime   = level.inttime + 20000;
        m_vNewCuriousPos = vPos;
        break;
    }
}

/*
====================
ClearEnemy

Clear the bot's enemy
====================
*/
void BotController::ClearEnemy(void)
{
    m_iAttackTime   = 0;
    m_pEnemy        = NULL;
    m_iEnemyEyesTag = -1;
    m_vOldEnemyPos  = vec_zero;
    m_vLastEnemyPos = vec_zero;
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

void BotController::CheckStates(void)
{
    m_StateCount = 0;

    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        botfunc_t *func = &botfuncs[i];

        if (func->CheckCondition) {
            if ((this->*func->CheckCondition)()) {
                if (!(m_StateFlags & (1 << i))) {
                    m_StateFlags |= 1 << i;

                    if (func->BeginState) {
                        (this->*func->BeginState)();
                    }
                }

                if (func->ThinkState) {
                    m_StateCount++;
                    (this->*func->ThinkState)();
                }
            } else {
                if ((m_StateFlags & (1 << i))) {
                    m_StateFlags &= ~(1 << i);

                    if (func->EndState) {
                        (this->*func->EndState)();
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
}

/*
====================
Idle state

Make the bot move to random directions
====================
*/
void BotController::InitState_Idle(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Idle;
    func->ThinkState     = &BotController::State_Idle;
}

bool BotController::CheckCondition_Idle(void)
{
    if (m_iCuriousTime) {
        return false;
    }

    if (m_iAttackTime) {
        return false;
    }

    return true;
}

void BotController::State_Idle(void)
{
    if (CheckWindows()) {
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_iLastFireTime = level.inttime;
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        CheckReload();
    }

    //
    // Added in OPM
    //  Human-like idle behavior: periodic pauses to look around
    //
    if (m_bIdlePausing) {
        // Currently paused - look around
        if (level.inttime >= m_iIdlePauseTime) {
            // Done pausing, resume movement
            m_bIdlePausing = false;
            // Sometimes start walking instead of running after a pause
            if (rand() % 4 == 0) {
                m_bWalking  = true;
                m_iWalkTime = level.inttime + 2000 + (int)G_Random(3000);
            }
        } else {
            // Look around periodically during pause
            if (level.inttime >= m_iIdleLookTime) {
                m_iIdleLookTime = level.inttime + 800 + (int)G_Random(1200);

                // Pick a random look direction
                Vector lookAngles = controlledEnt->angles;
                lookAngles.y += G_CRandom(90);
                lookAngles.x = G_CRandom(15);
                rotation.SetTargetAngles(lookAngles);
            }
            return;
        }
    } else {
        // Check if we should start a pause
        if (rand() % 400 == 0 && !movement.MoveToBestAttractivePoint(1)) {
            m_bIdlePausing   = true;
            m_iIdlePauseTime = level.inttime + 1500 + (int)G_Random(2500);
            m_iIdleLookTime  = level.inttime + 500;
            movement.ClearMove();
            return;
        }
    }

    //
    // Added in OPM
    //  Occasionally walk instead of run
    //
    if (m_bWalking && level.inttime >= m_iWalkTime) {
        m_bWalking = false;
    }

    // Changed in OPM
    //  Pre-aim toward highest-belief direction when not in combat and
    //  the zone is visible. Otherwise look along the path direction so
    //  the bot doesn't stare at walls.
    {
        Vector beliefPos = beliefMap.GetHighestBeliefPos();
        if (beliefPos != vec_zero && controlledEnt->CanSee(beliefPos, 120, 2048, false)) {
            rotation.AimAt(beliefPos);
        } else {
            AimAtAimNode();
        }
    }

    // Changed in OPM
    //  Belief-driven patrol: move toward the highest-belief zone instead
    //  of wandering randomly. Falls back to attractive nodes and random
    //  movement when no zone has significant belief.
    if (!movement.MoveToBestAttractivePoint() && !movement.IsMoving()) {
        Vector beliefPos = beliefMap.GetHighestBeliefPos();
        if (beliefPos != vec_zero) {
            movement.MoveTo(beliefPos);

            if (movement.MoveDone()) {
                beliefMap.ClearZone(beliefPos);
            }
        } else if (m_vLastDeathPos != vec_zero) {
            movement.MoveTo(m_vLastDeathPos);

            if (movement.MoveDone()) {
                m_vLastDeathPos = vec_zero;
            }
        } else {
            Vector randomDir(G_CRandom(16), G_CRandom(16), G_CRandom(16));
            Vector preferredDir;
            float  radius = 512 + G_Random(2048);

            preferredDir += Vector(controlledEnt->orientation[0]) * (rand() % 5 ? 1024 : -1024);
            preferredDir += Vector(controlledEnt->orientation[2]) * (rand() % 5 ? 1024 : -1024);
            movement.AvoidPath(controlledEnt->origin + randomDir, radius, preferredDir);
        }
    }
}

/*
====================
Curious state

Forward to the last event position
====================
*/
void BotController::InitState_Curious(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Curious;
    func->ThinkState     = &BotController::State_Curious;
}

bool BotController::CheckCondition_Curious(void)
{
    if (m_iAttackTime) {
        m_iCuriousTime = 0;
        return false;
    }

    if (level.inttime > m_iCuriousTime) {
        if (m_iCuriousTime) {
            movement.ClearMove();
            m_iCuriousTime = 0;
        }

        return false;
    }

    return true;
}

void BotController::State_Curious(void)
{
    if (CheckWindows()) {
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_iLastFireTime = level.inttime;
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    }

    // Changed in OPM
    //  Pre-aim toward highest-belief zone during curious state, but only
    //  when the zone is visible. Otherwise look along the path direction
    //  so the bot doesn't stare at walls.
    {
        Vector beliefPos = beliefMap.GetHighestBeliefPos();
        if (beliefPos != vec_zero && controlledEnt->CanSee(beliefPos, 120, 2048, false)) {
            rotation.AimAt(beliefPos);
        } else {
            AimAtAimNode();
        }
    }

    // Changed in OPM
    //  Navigate to the highest-belief zone instead of the last single
    //  event position. Multiple events build up belief in an area rather
    //  than replacing each other. Falls back to m_vNewCuriousPos when no
    //  zone has significant belief.
    if (!movement.MoveToBestAttractivePoint(3)) {
        Vector beliefPos = beliefMap.GetHighestBeliefPos();
        Vector targetPos = (beliefPos != vec_zero) ? beliefPos : m_vNewCuriousPos;

        if (!movement.IsMoving() || m_vLastCuriousPos != targetPos) {
            movement.MoveTo(targetPos);
            m_vLastCuriousPos = targetPos;
        }
    }

    if (movement.MoveDone()) {
        // Clear the belief at the arrived zone
        beliefMap.ClearZone(controlledEnt->origin);
        m_iCuriousTime = 0;
    }
}

/*
====================
Attack state

Attack the enemy
====================
*/
void BotController::InitState_Attack(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Attack;
    func->EndState       = &BotController::State_EndAttack;
    func->ThinkState     = &BotController::State_Attack;
}

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

    if (sent->IsSubclassOfPlayer()) {
        Player *player = static_cast<Player *>(sent);

        if (g_gametype->integer >= GT_TEAM && player->GetTeam() == controlledEnt->GetTeam()) {
            return false;
        }
    } else {
        if (sent->m_Team == controlledEnt->m_Team) {
            return false;
        }
    }

    return true;
}

bool BotController::CheckCondition_Attack(void)
{
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0;
    Sentient             *bestEnemy   = NULL;
    float                 bestDistSq  = 999999999.0f;

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);

    maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828);

    //
    // Changed in OPM
    //  Scan ALL visible enemies and pick the closest one, rather than
    //  returning early when any enemy is found. Also use wider FOV (120°)
    //  to notice threats from peripheral vision.
    //
    for (int i = 1; i <= sents.NumObjects(); i++) {
        Sentient *sent = sents.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        float distSq = (sent->origin - controlledEnt->origin).lengthSquared();

        // Use wider FOV (120°) to notice peripheral threats
        if (controlledEnt->CanSee(sent, 120, maxDistance, false)) {
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestEnemy  = sent;
            }
        }
    }

    // If we found a visible enemy, target them
    if (bestEnemy) {
        if (m_pEnemy != bestEnemy) {
            m_iEnemyEyesTag = -1;
            // Reset reaction time when switching targets
            m_iLastUnseenTime = level.inttime;
        }

        m_pEnemy        = bestEnemy;
        m_vLastEnemyPos = m_pEnemy->origin;
        m_iAttackTime   = level.inttime + 500 + (int)G_Random(1000);

        // Added in OPM
        //  Update belief map with direct sighting
        beliefMap.UpdateFromSighting(m_pEnemy->origin);

        return true;
    }

    // No visible enemy - check if we should keep hunting the last known position
    if (level.inttime > m_iAttackTime) {
        if (m_iAttackTime) {
            movement.ClearMove();
            m_iAttackTime = 0;
        }

        return false;
    }

    return true;
}

void BotController::State_EndAttack(void)
{
    m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    m_botCmd.rightmove = 0;
    m_botCmd.upmove    = 0;
    m_iStrafeTime      = 0;
    m_iStrafeDir       = 0;
    m_bStandingStill   = false;
    m_bCrouching       = false;
    m_bCrouchDecided   = false;
    m_iLeanDir         = 0;
    controlledEnt->ZoomOff();
}

void BotController::State_Attack(void)
{
    bool    bMelee              = false;
    bool    bCanSee             = false;
    bool    bCanAttack          = false;
    float   fMinDistance        = 128;
    float   fMinDistanceSquared = fMinDistance * fMinDistance;
    float   fEnemyDistanceSquared;
    Weapon *pWeap   = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    bool    bNoMove = false;
    bool    bFiring = false;

    // Changed in OPM
    //  When enemy is gone but we recently had one, keep looking at the last
    //  known position briefly instead of snapping away. This prevents the
    //  jarring 180-degree turn after a kill.
    // Changed in OPM
    //  When enemy is gone but we recently had one, keep looking at the last
    //  known position briefly instead of snapping away.
    if (!m_pEnemy || !IsValidEnemy(m_pEnemy)) {
        if (level.inttime < m_iAttackStopAimTime && m_vLastEnemyPos != vec_zero) {
            rotation.AimAt(m_vLastEnemyPos);
            m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
            m_iAttackTime = level.inttime + 200 + (int)G_Random(300);
            return;
        }

        m_iAttackTime = 0;
        return;
    }
    float fDistanceSquared = (m_pEnemy->origin - controlledEnt->origin).lengthSquared();

    m_vOldEnemyPos = m_vLastEnemyPos;

    bCanSee =
        controlledEnt->CanSee(m_pEnemy, 20, Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828), false);

    if (bCanSee) {
        if (!pWeap) {
            return;
        }

        bCanAttack = true;
        if (m_iLastUnseenTime) {
            const float        reactionTime = Q_min(1000 * Q_min(1, fDistanceSquared / Square(2048)), 1000);
            const unsigned int minDelay     = g_bot_attack_react_min_delay->value * 1000;
            const unsigned int randomDelay  = g_bot_attack_react_random_delay->value * 1000;
            if (level.inttime <= m_iLastUnseenTime + minDelay + G_Random(randomDelay)) {
                bCanAttack = false;
            } else {
                m_iLastUnseenTime = 0;
            }
        }

        if (bCanAttack) {
            const int fireDelay                    = pWeap->FireDelay(FIRE_PRIMARY) * 1000;
            float     fPrimaryBulletRange          = pWeap->GetBulletRange(FIRE_PRIMARY) / 1.25f;
            float     fPrimaryBulletRangeSquared   = fPrimaryBulletRange * fPrimaryBulletRange;
            float     fSecondaryBulletRange        = pWeap->GetBulletRange(FIRE_SECONDARY);
            float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;
            float     fSpreadFactor                = pWeap->GetSpreadFactor(FIRE_PRIMARY);

            const int maxcontinuousFireTime = fireDelay + g_bot_attack_continuousfire_min_firetime->value * 1000
                                            + G_Random(g_bot_attack_continuousfire_random_firetime->value * 1000);
            const int maxBurstTime = fireDelay + g_bot_attack_burst_min_time->value * 1000
                                   + G_Random(g_bot_attack_burst_random_delay->value * 1000);

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

            fMinDistance = fPrimaryBulletRange;

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

                if (!m_iLastBurstTime && m_iContinuousFireTime > maxcontinuousFireTime) {
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

            m_iAttackTime        = level.inttime + 500 + (int)G_Random(1000);
            m_iAttackStopAimTime = level.inttime + 500 + (int)G_Random(1000);
            m_iLastSeenTime      = level.inttime;
            m_vLastEnemyPos      = m_pEnemy->origin;
        }
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        fMinDistanceSquared = 0;

        if (level.inttime > m_iLastSeenTime + 2000) {
            m_iLastUnseenTime = level.inttime;
        }
    }

    if (bCanSee || level.inttime < m_iAttackStopAimTime) {
        Vector        vRandomOffset;
        Vector        vTarget;
        orientation_t eyes_or;

        if (m_iEnemyEyesTag == -1) {
            // Cache the tag
            m_iEnemyEyesTag = gi.Tag_NumForName(m_pEnemy->edict->tiki, "eyes bone");
        }

        if (m_iEnemyEyesTag != -1) {
            // Use the enemy's eyes bone
            m_pEnemy->GetTag(m_iEnemyEyesTag, &eyes_or);

            //vRandomOffset = Vector(G_CRandom(8), G_CRandom(8), -G_Random(32));
            vTarget = eyes_or.origin;
        } else {
            //vRandomOffset = Vector(G_CRandom(8), G_CRandom(8), 16 + G_Random(m_pEnemy->viewheight - 16));
            vTarget = m_pEnemy->origin;
        }

        //
        // Changed in OPM
        //  Humanized aiming: pick a new random offset target every 300-600ms,
        //  then smoothly lerp toward it. Scale offset magnitude by distance
        //  so bots are more accurate at close range.
        //
        if (level.inttime >= m_iLastAimTime + 300 + (int)G_Random(300)) {
            float halfW = (m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5;
            float halfD = (m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5;

            // Scale offset by distance: close (< 256) = tight, far (> 1024) = full spread
            float fDist     = sqrt(fDistanceSquared);
            float distScale = Q_clamp_float((fDist - 256) / 768, 0.15, 1.0);

            if (m_iEnemyEyesTag != -1) {
                m_vAimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                m_vAimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                m_vAimOffsetTarget[2] = -G_Random(m_pEnemy->maxs.z * 0.5) * distScale;
            } else {
                m_vAimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                m_vAimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                m_vAimOffsetTarget[2] = 16 + G_Random(m_pEnemy->viewheight - 16) * distScale;
            }

            m_iLastAimTime      = level.inttime;
            m_iAimLerpStartTime = level.inttime;
        }

        // Smoothly lerp current offset toward target offset
        {
            float dt       = level.frametime * g_bot_aim_lerp_speed->value;
            float lerpFrac = Q_clamp_float(dt, 0.0, 1.0);

            m_vAimOffset[0] = m_vAimOffset[0] + (m_vAimOffsetTarget[0] - m_vAimOffset[0]) * lerpFrac;
            m_vAimOffset[1] = m_vAimOffset[1] + (m_vAimOffsetTarget[1] - m_vAimOffset[1]) * lerpFrac;
            m_vAimOffset[2] = m_vAimOffset[2] + (m_vAimOffsetTarget[2] - m_vAimOffset[2]) * lerpFrac;
        }

        rotation.AimAt(vTarget + m_vAimOffset * g_bot_attack_spreadmult->value);
    } else {
        AimAtAimNode();
    }

    if (bNoMove) {
        m_bStandingStill = true;
        return;
    }

    fEnemyDistanceSquared = (controlledEnt->origin - m_vLastEnemyPos).lengthSquared();

    //
    // Added in OPM
    //  Stand still to aim at long range targets (more accurate)
    //  At close range, keep moving
    //
    const float longRangeThreshold = 800 * 800;
    const float midRangeThreshold  = 400 * 400;

    if (bCanSee && bFiring && fEnemyDistanceSquared > longRangeThreshold) {
        // Long range: stop forward movement, strafing handled separately
        m_bStandingStill = true;
        movement.ClearMove();
    } else if (bCanSee && bFiring && fEnemyDistanceSquared > midRangeThreshold) {
        // Mid range: stop periodically to aim, then move
        if (rand() % 100 < 30) {
            m_bStandingStill = true;
            movement.ClearMove();
        } else {
            m_bStandingStill = false;
        }
    } else {
        m_bStandingStill = false;
    }

    //
    // Added in OPM
    //  Leaning during combat: periodically lean left/right when stationary
    //
    if (bCanSee && m_bStandingStill) {
        if (level.inttime >= m_iLeanTime) {
            m_iLeanTime = level.inttime + 1500 + (int)G_Random(2000);

            // Pick lean direction: left, right, or none
            int roll = rand() % 5;
            if (roll < 2) {
                m_iLeanDir = -1;
            } else if (roll < 4) {
                m_iLeanDir = 1;
            } else {
                m_iLeanDir = 0;
            }
        }
    } else {
        // Not standing still, don't lean
        m_iLeanDir = 0;
    }

    //
    // Added in OPM
    //  Combat crouching: crouch when standing still to reduce profile
    //  and improve accuracy. Decided once when entering standing-still state.
    //
    if (m_bStandingStill) {
        if (!m_bCrouching && !m_bCrouchDecided) {
            m_bCrouchDecided = true;
            if (rand() % 100 < g_bot_crouch_chance->integer) {
                m_bCrouching = true;
            }
        }
    } else {
        m_bCrouching     = false;
        m_bCrouchDecided = false;
    }

    if (m_bCrouching) {
        m_botCmd.upmove = -127;
    } else {
        m_botCmd.upmove = 0;
    }

    //
    // Changed in OPM
    //  Combat strafing: ADAD spam when actively firing and visible to enemy.
    //  Placed before the standing-still return so bots strafe at all ranges,
    //  even when holding position.
    //
    // Changed in OPM
    //  Combat strafing with varied, unpredictable timing.
    //  Mix of quick direction changes, longer holds, and brief pauses
    //  so the pattern is never consistent.
    if (bCanSee && !bMelee) {
        if (level.inttime >= m_iStrafeTime) {
            int roll = rand() % 10;

            if (roll < 2) {
                // Quick tap: short hold, then switch
                m_iStrafeTime = level.inttime + 150 + (int)G_Random(250);
                m_iStrafeDir  = (rand() % 2) ? 127 : -127;
            } else if (roll < 4) {
                // Hold direction: commit to one side for a while
                m_iStrafeTime = level.inttime + 600 + (int)G_Random(1200);
                m_iStrafeDir  = (rand() % 2) ? 127 : -127;
            } else if (roll < 8) {
                // Pause: stop strafing, longer duration
                m_iStrafeTime = level.inttime + 300 + (int)G_Random(700);
                m_iStrafeDir  = 0;
            } else {
                // Double-tap: reverse current direction
                m_iStrafeTime = level.inttime + 100 + (int)G_Random(200);
                m_iStrafeDir  = m_iStrafeDir > 0 ? -127 : 127;
            }
        }

        m_botCmd.rightmove = m_iStrafeDir;
    }

    if (m_bStandingStill) {
        return;
    }

    // Changed in OPM
    //  Combat movement: when the bot can see and attack, hold position and
    //  fight from the current range — strafing provides lateral movement.
    //  Never flee from close combat; at point blank, stand and shoot.
    //  Only advance when the enemy is not visible or when using melee.
    if (bCanSee && bCanAttack && !bMelee) {
        // Can see and shoot — hold position, let strafing handle movement
        if (movement.IsMoving() && !movement.MoveToBestAttractivePoint(5)) {
            movement.ClearMove();
        }
    } else if ((!movement.MoveToBestAttractivePoint(5) && !movement.IsMoving())
               || (m_vOldEnemyPos != m_vLastEnemyPos && !movement.MoveDone())) {
        // Can't see enemy or using melee — close the distance
        movement.MoveTo(m_vLastEnemyPos);

        if (!bCanSee && movement.MoveDone()) {
            // Lost track of the enemy
            ClearEnemy();
            return;
        }
    }

    if (movement.IsMoving()) {
        m_iAttackTime = level.inttime + 500 + (int)G_Random(1000);
    }
}

/*
====================
Grenade state

Avoid any grenades
====================
*/
void BotController::InitState_Grenade(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Grenade;
    func->ThinkState     = &BotController::State_Grenade;
}

bool BotController::CheckCondition_Grenade(void)
{
    // Added in OPM
    //  Scan for nearby enemy projectiles (grenades) and flee from them
    if (m_pGrenade && m_pGrenade->IsSubclassOfProjectile()) {
        float distSq = (m_pGrenade->origin - controlledEnt->origin).lengthSquared();
        float radius = g_bot_grenade_avoid_radius->value;

        if (distSq < radius * radius) {
            return true;
        }
    }

    m_pGrenade = NULL;

    float      radiusSq = Square(g_bot_grenade_avoid_radius->value);
    gentity_t *edict;
    int        i;

    for (i = game.maxclients, edict = &g_entities[i]; i < globals.num_entities; i++, edict++) {
        if (!edict->inuse || !edict->entity) {
            continue;
        }

        Entity *ent = edict->entity;
        if (!ent->IsSubclassOfProjectile()) {
            continue;
        }

        Projectile *proj = static_cast<Projectile *>(ent);

        // Ignore own projectiles
        if (proj->GetOwner() == controlledEnt) {
            continue;
        }

        // Ignore friendly projectiles in team games
        Sentient *projOwner = proj->GetOwner();
        if (projOwner && projOwner->IsSubclassOfPlayer() && g_gametype->integer >= GT_TEAM) {
            Player *p = static_cast<Player *>(projOwner);
            if (p->GetTeam() == controlledEnt->GetTeam()) {
                continue;
            }
        }

        float distSq = (ent->origin - controlledEnt->origin).lengthSquared();
        if (distSq < radiusSq) {
            m_pGrenade          = ent;
            m_iGrenadeAvoidTime = level.inttime + 3000;
            return true;
        }
    }

    if (level.inttime < m_iGrenadeAvoidTime) {
        return true;
    }

    return false;
}

void BotController::State_Grenade(void)
{
    // Added in OPM
    //  Flee away from the grenade
    if (!m_pGrenade) {
        return;
    }

    Vector grenadePos = m_pGrenade->origin;
    Vector fleeDir    = controlledEnt->origin - grenadePos;
    VectorNormalizeFast(fleeDir);

    movement.AvoidPath(grenadePos, g_bot_grenade_avoid_radius->value, fleeDir * 512);
}

/*
====================
Weapon state

Change weapon when necessary
====================
*/
void BotController::InitState_Weapon(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Weapon;
    func->BeginState     = &BotController::State_BeginWeapon;
}

bool BotController::CheckCondition_Weapon(void)
{
    return controlledEnt->GetActiveWeapon(WEAPON_MAIN)
        != controlledEnt->BestWeapon(NULL, false, WEAPON_CLASS_THROWABLE);
}

void BotController::State_BeginWeapon(void)
{
    Weapon *weap = controlledEnt->BestWeapon(NULL, false, WEAPON_CLASS_THROWABLE);

    if (weap == NULL) {
        SendCommand("safeholster 1");
        return;
    }

    SendCommand(va("use \"%s\"", weap->model.c_str()));
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

    // Added in OPM
    //  Record death location in belief map — persists across respawn
    beliefMap.UpdateFromDeath(controlledEnt->origin);

    if (attacker && rand() % 5 == 0) {
        // 1/5 chance to go back to the attacker position
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
    Info_SetValueForKey(controlledEnt->client->pers.userinfo, "dm_playermodel", G_GetRandomAlliedPlayerModel());
    Info_SetValueForKey(controlledEnt->client->pers.userinfo, "dm_playergermanmodel", G_GetRandomGermanPlayerModel());

    G_ClientUserinfoChanged(controlledEnt->edict, controlledEnt->client->pers.userinfo);
}

void BotController::GotKill(const Event& ev)
{
    //
    // Changed in OPM
    //  Don't fully exit combat state after a kill - just clear the current
    //  enemy so CheckCondition_Attack can find a new target. Keep m_iAttackTime
    //  active so the bot stays in combat mode and continues scanning for enemies.
    //
    m_pEnemy        = NULL;
    m_iEnemyEyesTag = -1;
    m_iCuriousTime  = 0;

    // Extend attack time briefly to allow scanning for new targets
    if (m_iAttackTime) {
        m_iAttackTime = level.inttime + 500 + (int)G_Random(1000);
    }

    if (g_bot_instamsg_chance->integer && level.inttime >= m_iNextTauntTime
        && (rand() % g_bot_instamsg_chance->integer) == 0) {
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

        m_iNextTauntTime = level.inttime + g_bot_instamsg_delay->integer;
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

    // Added in OPM
    //  Initialize belief map from world bounds
    if (world && !beliefMap.IsInitialized()) {
        beliefMap.Init(world->absmin, world->absmax, 512.0f);
    }

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
