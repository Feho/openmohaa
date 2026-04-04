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
#include "dm_manager.h"
#include "playerstart.h"
#include "scriptexception.h"
#include "vehicleturret.h"
#include "weaputils.h"
#include "g_bot.h"

// We assume that we have limited access to the server-side
// and that most logic come from the playerstate_s structure

CLASS_DECLARATION(Listener, BotController, NULL) {
    {NULL, NULL}
};

// Added in OPM
//  Personality preset pool. Each bot draws one randomly at spawn.
//  Traits are 0.0-1.0 floats. Model strings are substrings matched
//  against the model list; NULL means random.
const BotPersonality botPersonalityPool[] = {
    // name        accuracy  aggression  patience  stealth  weaponClass            alliedModel  germanModel
    {"default",    0.5f,     0.5f,       0.5f,     0.0f,    0,                     NULL,        NULL       },
    {"sniper",     0.8f,     0.3f,       0.9f,     0.3f,    WEAPON_CLASS_RIFLE,    NULL,        NULL       },
    {"rusher",     0.4f,     0.9f,       0.1f,     0.0f,    WEAPON_CLASS_SMG,      NULL,        NULL       },
    {"gunner",     0.5f,     0.6f,       0.6f,     0.0f,    WEAPON_CLASS_MG,       NULL,        NULL       },
    {"stealth",    0.6f,     0.4f,       0.5f,     0.9f,    WEAPON_CLASS_SMG,      NULL,        NULL       },
};

const int botPersonalityPoolSize = sizeof(botPersonalityPool) / sizeof(botPersonalityPool[0]);

const BotPersonality& G_GetRandomBotPersonality()
{
    return botPersonalityPool[rand() % botPersonalityPoolSize];
}

// Added in OPM
//  Initialize per-bot parameters from global cvars.
void BotParams::InitFromCvars()
{
    turnSpeed       = g_bot_turn_speed->integer;
    aimOvershoot    = g_bot_aim_overshoot->value;
    aimSettleSpeed  = g_bot_aim_settle_speed->value;
    aimNoise        = g_bot_aim_noise->value;
    aimLerpSpeed    = g_bot_aim_lerp_speed->value;

    attackReactMinDelay          = g_bot_attack_react_min_delay->value;
    attackReactRandomDelay       = g_bot_attack_react_random_delay->value;
    attackBurstMinTime           = g_bot_attack_burst_min_time->value;
    attackBurstRandomDelay       = g_bot_attack_burst_random_delay->value;
    attackContinuousFireMinTime  = g_bot_attack_continuousfire_min_firetime->value;
    attackContinuousFireRandomTime = g_bot_attack_continuousfire_random_firetime->value;
    attackSpreadMult             = g_bot_attack_spreadmult->value;
    crouchChance                 = g_bot_crouch_chance->integer;

    grenadeAvoidRadius = g_bot_grenade_avoid_radius->value;

    beliefDecay         = g_bot_belief_decay->value;
    beliefEventWeight   = g_bot_belief_event_weight->value;
    beliefMinPatrol     = g_bot_belief_min_patrol->value;
    beliefVisitPenalty  = g_bot_belief_visit_penalty->value;
    beliefNoveltyBonus  = g_bot_belief_novelty_bonus->value;
    beliefScoreJitter   = g_bot_belief_score_jitter->value;
    beliefVisitDecay    = g_bot_belief_visit_decay->value;
    beliefPathBlockTime = g_bot_belief_path_block_time->value;

    progressStallTime = g_bot_progress_stall_time->value;
    stuckRadius       = g_bot_stuck_radius->value;
    stuckTime         = g_bot_stuck_time->value;

    instamsgChance = g_bot_instamsg_chance->integer;
    instamsgDelay  = g_bot_instamsg_delay->value;
}

// Added in OPM
//  Modulate parameters based on personality traits.
//  Traits are 0.0-1.0 floats centered around 0.5 (default behavior).
void BotParams::ApplyPersonality(const BotPersonality& personality)
{
    // Accuracy: higher = less noise, less spread, faster settle, less overshoot
    float accuracyMult    = 1.5f - personality.accuracy;       // 1.0 at 0.5, 0.7 at 0.8
    float accuracyInvMult = 0.5f + personality.accuracy;       // 1.0 at 0.5, 1.3 at 0.8
    aimNoise         *= accuracyMult;
    aimOvershoot     *= accuracyMult;
    attackSpreadMult *= accuracyMult;
    aimSettleSpeed   *= accuracyInvMult;

    // Aggression: higher = faster reactions, shorter idle pauses
    float aggroMult    = 1.5f - personality.aggression;        // 1.0 at 0.5, 0.6 at 0.9
    float aggroInvMult = 0.5f + personality.aggression;        // 1.0 at 0.5, 1.4 at 0.9
    attackReactMinDelay    *= aggroMult;
    attackReactRandomDelay *= aggroMult;
    beliefEventWeight      *= aggroInvMult;

    // Patience: higher = longer idle behavior, more visit penalty (explores less)
    // Lower patience = shorter burst pauses (more aggressive firing)
    float patienceMult = 0.5f + personality.patience;          // 1.0 at 0.5, 1.4 at 0.9
    beliefVisitPenalty *= 1.5f - personality.patience;
    beliefNoveltyBonus *= 1.5f - personality.patience;

    // Stealth: higher = more crouching
    crouchChance = (int)(crouchChance * (0.5f + personality.stealth));
}

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

    // Reset grouped state
    m_combat.reset();
    m_enemy.reset();
    m_curious.reset();
    m_grenade.reset();
    m_idle.reset();

    m_iNextTauntTime   = 0;
    m_iLastFireTime    = 0;
    m_personalityIndex = -1;

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

const BotPersonality& BotController::GetPersonality() const
{
    return m_personality;
}

int BotController::GetPersonalityIndex() const
{
    return m_personalityIndex;
}

void BotController::SetPersonality(const BotPersonality& personality, int index)
{
    m_personality      = personality;
    m_personalityIndex = index;
}

// Added in OPM
//  Draw debug visualization of belief zones. Each zone is drawn as a
//  colored circle at its centroid: green (low) -> yellow -> red (high).
//  The current patrol target zone gets a cyan pyramid marker.
//  A cyan arrow is drawn from the bot to its target zone.
//  Only drawn for the first bot to avoid visual clutter.
//  Periodic console output summarizes belief state.
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

    const Container<BeliefZone>& zones    = beliefMap.GetZones();
    Vector                       botPos   = controlledEnt->origin;
    int                          bestZone = beliefMap.GetBestZone(botPos);

    int activeCount = 0;

    for (int i = 1; i <= zones.NumObjects(); i++) {
        const BeliefZone& zone = zones.ObjectAt(i);
        if (zone.belief < 0.01f) {
            continue;
        }

        activeCount++;

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

        // Mark the current patrol target with a cyan pyramid
        if ((i - 1) == bestZone) {
            Vector pyramidPos = zone.centroid;
            pyramidPos.z += 96.0f;
            G_DebugPyramid(pyramidPos, 48.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    // Draw arrow from bot to target zone
    if (bestZone >= 0 && bestZone < zones.NumObjects()) {
        const BeliefZone& target = zones.ObjectAt(bestZone + 1);
        Vector            dir    = target.centroid - botPos;
        float             len    = dir.length();
        if (len > 1.0f) {
            VectorNormalize(dir);
            G_DebugArrow(botPos, dir, len, 0.0f, 1.0f, 1.0f, 1.0f);
        }
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
    if (m_idle.pausing || m_combat.standingStill || m_idle.walking) {
        m_botCmd.buttons &= ~BUTTON_RUN;
    } else {
        m_botCmd.buttons |= BUTTON_RUN;
    }

    //
    // Added in OPM
    //  Handle leaning during combat
    //
    m_botCmd.buttons &= ~(BUTTON_LEAN_LEFT | BUTTON_LEAN_RIGHT);
    if (m_idle.leanDir < 0) {
        m_botCmd.buttons |= BUTTON_LEAN_LEFT;
    } else if (m_idle.leanDir > 0) {
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

    // Added in OPM
    //  If movement gave up trying to reach a destination, record that target as unreachable.
    //  Any future destination near that target will be rejected.
    if (movement.DidGiveUpPath()) {
        beliefMap.AddFailedTarget(movement.GetBlockedDestination());
        beliefMap.MarkPathBlocked(movement.GetBlockedDestination());
    }

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

    fRangeFactor = 1.0 - (fDistanceSquared / fRadiusSquared);
    if (fRangeFactor < 0) {
        fRangeFactor = 0;
    }

    //
    // Resolve the entity that caused the sound
    //
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
    //  Early exit for sounds from blocked areas - don't update belief map
    //  and don't set curious. This prevents bots from clustering near walls
    //  trying to investigate unreachable sounds.
    if (beliefMap.IsPathBlocked(vPos) || beliefMap.IsNearFailedTarget(vPos)) {
        return;
    }

    // Added in OPM
    //  If we recently failed to reach a curious target, ignore all sounds for a few seconds.
    //  This prevents the bot from immediately chasing new sounds from the same unreachable area.
    if (m_curious.cooldownTime > level.inttime) {
        return;
    }

    // Changed in OPM
    //  Always update beliefs from enemy sounds regardless of distance
    //  or whether the bot is already investigating something else.
    //  The belief map is the bot's spatial awareness — it should
    //  accumulate all heard information. Only the curiosity trigger
    //  (whether to actively investigate) is gated by probability.
    beliefMap.UpdateFromEvent(vPos, iType, fRangeFactor);

    //
    // Curiosity trigger — probability-gated
    //

    // Already curious about a closer sound? Don't switch target.
    if (m_curious.time) {
        delta1 = vPos - controlledEnt->origin;
        delta2 = m_curious.targetPos - controlledEnt->origin;
        if (delta1.lengthSquared() > delta2.lengthSquared()) {
            return;
        }
    }

    // Changed in OPM
    //  Close-range weapon fire and explosions bypass the probability gate.
    //  This ensures bots react immediately to nearby threats even if they
    //  weren't directly hit. A soldier would always notice gunfire 10 feet away.
    //  Note: WEAPON_IMPACT is excluded - we want the shooter position, not impact.
    bool bypassProbability = false;
    if (fRangeFactor > 0.7f) {
        // Close range (within ~30% of sound radius)
        switch (iType) {
        case AI_EVENT_WEAPON_FIRE:
        case AI_EVENT_EXPLOSION:
        case AI_EVENT_GRENADE:
            bypassProbability = true;
            break;
        default:
            break;
        }
    }

    if (!bypassProbability && fRangeFactor < random()) {
        return;
    }

    switch (iType) {
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
    case AI_EVENT_WEAPON_IMPACT:
        // Ignore bullet impacts - they indicate where the bullet hit, not where
        // the shooter is. React to WEAPON_FIRE instead which gives shooter position.
        break;
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_EXPLOSION:
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
    case AI_EVENT_FOOTSTEP:
    case AI_EVENT_GRENADE:
    default:
        m_curious.time      = level.inttime + 20000;
        m_curious.targetPos = vPos;
        break;
    }

    // Added in OPM
    //  For close-range threat sounds, immediately turn toward the source.
    //  Don't wait for the state machine to process it - a soldier would
    //  instinctively look toward nearby gunfire. Skip if already in combat
    //  (attack state handles its own aiming).
    if (bypassProbability && !m_combat.attackTime) {
        if (g_bot_debug_reaction->integer) {
            gi.Printf(
                "BOT %s: Immediate reaction to close-range sound (type=%d, rangeFactor=%.2f) at (%.0f, %.0f, %.0f)\n",
                controlledEnt->client->pers.netname,
                iType,
                fRangeFactor,
                vPos.x,
                vPos.y,
                vPos.z
            );
        }
        rotation.AimAt(vPos);
        movement.ClearMove();
        m_idle.reset();
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
    m_combat.attackTime = 0;
    m_enemy.enemy       = NULL;
    m_enemy.eyesTag     = -1;
    m_enemy.oldPos      = vec_zero;
    m_enemy.lastPos     = vec_zero;
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

    // Added in OPM
    //  First pass: try to find a weapon matching the personality's preferred class
    int preferredClass = m_personality.preferredWeaponClass;
    if (preferredClass) {
        bestweapon = NULL;
        bestrank   = -999999;

        for (j = 1; j <= n; j++) {
            next = (Weapon *)G_GetEntity(inventory.ObjectAt(j));

            assert(next);
            if (!next->IsSubclassOfWeapon() || next->IsSubclassOfInventoryItem()) {
                continue;
            }

            if (!(next->GetWeaponClass() & preferredClass)) {
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

        if (bestweapon) {
            return bestweapon;
        }
    }

    // Fallback: search for the best weapon with ammo regardless of class
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
    m_curious.time   = 0;
    m_botCmd.buttons = 0;

    // Added in OPM
    //  Seed belief map with enemy spawn points so bots patrol toward
    //  likely spawn areas on round start.
    beliefMap.SeedFromSpawnPoints(controlledEnt);
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
        m_enemy.deathPos = attacker->origin;
    } else {
        m_enemy.deathPos = vec_zero;
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

// Added in OPM
//  React to being damaged - look toward attacker immediately
void BotController::Damaged(const Event& ev)
{
    Entity *attacker = ev.GetEntity(1);

    if (!attacker || attacker == controlledEnt) {
        return;
    }

    // Check if attacker is a valid enemy
    Sentient *sentAttacker = NULL;
    if (attacker->IsSubclassOfSentient()) {
        sentAttacker = static_cast<Sentient *>(attacker);

        if (!IsValidEnemy(sentAttacker)) {
            return;
        }
    }

    // Update belief map with attacker position - high confidence
    beliefMap.UpdateFromEvent(attacker->origin, AI_EVENT_WEAPON_FIRE, 1.0f);

    // If we already have this enemy targeted and can see them, don't interrupt
    if (m_enemy.enemy == sentAttacker && m_combat.lastSeenTime == level.inttime) {
        return;
    }

    // Immediately look toward the attacker
    rotation.AimAt(attacker->centroid);

    // If the attacker is a valid sentient enemy, enter attack mode
    if (sentAttacker) {
        if (g_bot_debug_reaction->integer) {
            gi.Printf(
                "BOT %s: Damaged by %s - entering attack mode, looking at (%.0f, %.0f, %.0f)\n",
                controlledEnt->client->pers.netname,
                sentAttacker->IsSubclassOfPlayer() ? static_cast<Player *>(sentAttacker)->client->pers.netname
                                                   : sentAttacker->targetname.c_str(),
                attacker->centroid.x,
                attacker->centroid.y,
                attacker->centroid.z
            );
        }

        // Set up enemy tracking
        m_enemy.enemy   = sentAttacker;
        m_enemy.lastPos = sentAttacker->origin;
        m_enemy.eyesTag = gi.Tag_NumForName(sentAttacker->edict->tiki, "eyes bone");

        // Enter attack state - still need reaction time to aim before firing
        // Being shot tells you where the threat is, but you still need to turn and aim
        m_combat.attackTime        = level.inttime + 5000;
        m_combat.lastSeenTime      = level.inttime;
        m_combat.attackStopAimTime = level.inttime + 2000;
        m_combat.lastUnseenTime    = level.inttime; // Start reaction delay - need time to aim

        // Clear movement so we don't keep walking away from threat
        movement.ClearMove();

        // Clear any curious state - we have a real threat now
        m_curious.time = 0;
    } else {
        // Non-sentient attacker (e.g., explosion, trap) - go curious toward the position
        m_curious.targetPos = attacker->origin;
        m_curious.time      = level.inttime + 5000;
    }
}

void BotController::GotKill(const Event& ev)
{
    //
    // Changed in OPM
    //  Don't fully exit combat state after a kill - just clear the current
    //  enemy so CheckCondition_Attack can find a new target. Keep m_combat.attackTime
    //  active so the bot stays in combat mode and continues scanning for enemies.
    //
    m_enemy.enemy   = NULL;
    m_enemy.eyesTag = -1;
    m_curious.time  = 0;

    // Extend attack time briefly to allow scanning for new targets
    if (m_combat.attackTime) {
        m_combat.attackTime = level.inttime + 500 + (int)G_Random(1000);
    }

    if (m_params.instamsgChance && level.inttime >= m_iNextTauntTime
        && (rand() % m_params.instamsgChance) == 0) {
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

        m_iNextTauntTime = level.inttime + (int)(m_params.instamsgDelay);
    }
}

void BotController::EventStuffText(const str& text)
{
    SendCommand(text);
}

void BotController::setControlledEntity(Player *player)
{
    controlledEnt = player;

    m_params.InitFromCvars();
    m_params.ApplyPersonality(m_personality);
    movement.SetControlledEntity(player, &m_params);
    rotation.SetControlledEntity(player, &m_params);
    beliefMap.SetParams(&m_params);

    // Added in OPM
    //  Initialize belief map from spawn point bounds. We use spawn points
    //  rather than world->absmin/absmax because the world entity bounds
    //  don't reflect the actual playable area.
    if (!beliefMap.IsInitialized()) {
        Vector mapMins(999999, 999999, 999999);
        Vector mapMaxs(-999999, -999999, -999999);
        int    totalSpawns = 0;

        DM_Team *teams[] = {dmManager.GetTeamAllies(), dmManager.GetTeamAxis()};
        for (int t = 0; t < 2; t++) {
            for (int i = 1; i <= teams[t]->m_spawnpoints.NumObjects(); i++) {
                PlayerStart *spawn = teams[t]->m_spawnpoints.ObjectAt(i);
                Vector       pos   = spawn->origin;

                if (pos.x < mapMins.x) {
                    mapMins.x = pos.x;
                }
                if (pos.y < mapMins.y) {
                    mapMins.y = pos.y;
                }
                if (pos.z < mapMins.z) {
                    mapMins.z = pos.z;
                }
                if (pos.x > mapMaxs.x) {
                    mapMaxs.x = pos.x;
                }
                if (pos.y > mapMaxs.y) {
                    mapMaxs.y = pos.y;
                }
                if (pos.z > mapMaxs.z) {
                    mapMaxs.z = pos.z;
                }
                totalSpawns++;
            }
        }

        if (totalSpawns > 0) {
            // Pad bounds so edge spawns aren't at grid boundary
            float padding = 512.0f;
            mapMins.x -= padding;
            mapMins.y -= padding;
            mapMaxs.x += padding;
            mapMaxs.y += padding;

            beliefMap.Init(mapMins, mapMaxs, 512.0f);
        }
    }

    delegateHandle_gotKill =
        player->delegate_gotKill.Add(std::bind(&BotController::GotKill, this, std::placeholders::_1));
    delegateHandle_killed = player->delegate_killed.Add(std::bind(&BotController::Killed, this, std::placeholders::_1));
    delegateHandle_damage =
        player->delegate_damage.Add(std::bind(&BotController::Damaged, this, std::placeholders::_1));
    delegateHandle_stufftext =
        player->delegate_stufftext.Add(std::bind(&BotController::EventStuffText, this, std::placeholders::_1));
    delegateHandle_spawned = player->delegate_spawned.Add(std::bind(&BotController::Spawned, this));
}

Player *BotController::getControlledEntity() const
{
    return controlledEnt;
}

BotController *BotControllerManager::createController(Player *player, const BotPersonality& personality, int personalityIndex)
{
    BotController *controller = new BotController();
    controller->SetPersonality(personality, personalityIndex);
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
