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
    m_senses.reset();

    m_bFirstSpawn = true;

    m_iNextTauntTime = 0;
    m_iLastFireTime  = 0;

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

const BotMovement& BotController::GetMovement() const
{
    return movement;
}

BotMemory& BotController::GetMemory()
{
    return m_memory;
}

BotCoverageMap& BotController::GetCoverage()
{
    return m_coverage;
}

const BotProfile& BotController::GetProfile() const
{
    return m_profile;
}

const BotGoal& BotController::GetCurrentGoal() const
{
    return m_planner.Current();
}

// Added in OPM
//  Draw debug visualization of coverage and memory state.
//  Only drawn for the first bot to avoid visual clutter.
void BotController::DrawDebugCoverage()
{
    // Only draw for the first bot to avoid clutter
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();
    if (controllers.NumObjects() > 0 && controllers.ObjectAt(1) != this) {
        return;
    }

    m_coverage.DrawDebug(controlledEnt->origin, level.inttime);

    // Periodic console summary (every 2 seconds)
    static int lastPrintTime = 0;
    if (level.inttime - lastPrintTime >= 2000) {
        lastPrintTime = level.inttime;

        Vector memPos    = vec_zero;
        bool   hasMemory = m_memory.GetMostRelevantPos(controlledEnt->origin, level.inttime, memPos);
        gi.Printf(
            "--- Coverage [%s]: memory=%d events, bestMemory=(%.0f, %.0f, %.0f) ---\n",
            controlledEnt->client->pers.netname,
            m_memory.Count(),
            hasMemory ? memPos.x : vec_zero.x,
            hasMemory ? memPos.y : vec_zero.y,
            hasMemory ? memPos.z : vec_zero.z
        );
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
        // Primary weapon — use profile preference if available
        //
        event = new Event(EV_Player_PrimaryDMWeapon);
        event->AddString(m_profile.preferredWeapon.length() ? m_profile.preferredWeapon.c_str() : "auto");

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
    //  Per-frame memory maintenance and coverage tracking
    m_memory.Tick(level.inttime);
    UpdateCoverage();
    m_planner.Tick(level.inttime);

    CheckStates();

    movement.MoveThink(m_botCmd);
    rotation.TurnThink(m_botCmd, m_botEyes);
    CheckUse();

    CheckValidWeapon();

    // Added in OPM
    //  Debug visualization of coverage state
    if (g_bot_debug_coverage->integer) {
        DrawDebugCoverage();
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

// Added in OPM
//  Track which navigation nodes the bot has visited. Called each frame.
//  Finds the nearest navigation node each frame.
void BotController::UpdateCoverage(void)
{
    if (!controlledEnt || controlledEnt->IsDead()) {
        return;
    }

    PathNode *nearNode = PathSearch::NearestEndNode(controlledEnt->origin);
    if (nearNode) {
        m_coverage.MarkVisited(nearNode->nodenum, level.inttime);
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

    // Changed in OPM
    //  Always remember enemy sounds regardless of distance or whether
    //  the bot is already investigating something else.
    float eventWeight = 0.0f;
    switch (iType) {
    case AI_EVENT_WEAPON_FIRE:
        eventWeight = 0.6f;
        break;
    case AI_EVENT_EXPLOSION:
        eventWeight = 0.4f;
        break;
    case AI_EVENT_FOOTSTEP:
        eventWeight = 0.2f;
        break;
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
        eventWeight = 0.3f;
        break;
    case AI_EVENT_GRENADE:
        eventWeight = 0.5f;
        break;
    case AI_EVENT_WEAPON_IMPACT:
        eventWeight = 0.0f;
        break;
    default:
        eventWeight = 0.15f;
        break;
    }

    if (eventWeight > 0.0f) {
        m_memory.Remember(vPos, iType, eventWeight * fRangeFactor);
    }

    // Ignore bullet impact positions — they indicate where the bullet hit,
    // not where the shooter is. React to WEAPON_FIRE instead.
    if (iType == AI_EVENT_MISC || iType == AI_EVENT_MISC_LOUD || iType == AI_EVENT_WEAPON_IMPACT) {
        return;
    }

    // Refactored in OPM (see github issue #8)
    //  Perception writes only to the BotSenses struct and the belief map.
    //  The state machine reads m_senses each frame to decide whether to
    //  react, pre-aim, or transition state. This prevents perception
    //  events from competing with state `Think` functions for rotation
    //  and movement control.
    //
    //  Previously NoticeEvent set m_curious.time/targetPos, called
    //  rotation.AimAt, movement.ClearMove, and m_idle.reset directly —
    //  which was immediately overwritten by BotStateIdle::Think the next
    //  frame, causing bots to ignore gunfire from behind.

    // Keep the strongest recent sound: prefer higher range (closer), but
    // always refresh if the existing sense has decayed.
    const int senseMaxAge = 20000;
    if (level.inttime > m_senses.heardTime + senseMaxAge || fRangeFactor >= m_senses.heardRange) {
        m_senses.heardPos   = vPos;
        m_senses.heardType  = iType;
        m_senses.heardTime  = level.inttime;
        m_senses.heardRange = fRangeFactor;

        if (g_bot_debug_state->integer >= 2) {
            gi.Printf(
                "BOT %s: NoticeEvent -> senses (type=%d, range=%.2f, pos=(%.0f,%.0f,%.0f))\n",
                controlledEnt->client->pers.netname,
                iType,
                fRangeFactor,
                vPos.x,
                vPos.y,
                vPos.z
            );
        }
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
    m_curious.reset();
    m_botCmd.buttons = 0;
    m_planner.Reset();

    // Added in OPM
    //  Assign personality profile only on first spawn so the bot keeps
    //  the same profile (and weapon/model) across respawns within a map.
    if (m_bFirstSpawn) {
        m_profile = botProfileManager.PickProfile();
        rotation.SetAimParameters(
            m_profile.turnSpeed, m_profile.aimOvershoot, m_profile.aimSettleSpeed, m_profile.aimNoise
        );

        if (g_bot_debug_state->integer) {
            gi.Printf(
                "BOT %s: spawned with profile '%s'\n", controlledEnt->client->pers.netname, m_profile.name.c_str()
            );
        }

        m_bFirstSpawn = false;
    }

    // Added in OPM
    //  Clear memory on spawn but keep coverage across respawns so
    //  the bot doesn't re-explore areas it already covered.
    m_memory.Clear();
    m_senses.reset();
    m_curious.reset();
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
    //  Remember death location so the bot may return to investigate
    m_memory.Remember(controlledEnt->origin, AI_EVENT_WEAPON_FIRE, 0.8f);

    if (attacker && rand() % 5 == 0) {
        // 1/5 chance to go back to the attacker position
        m_enemy.deathPos = attacker->origin;
    } else {
        m_enemy.deathPos = vec_zero;
    }

    // Changed in OPM
    //  Use profile's preferred weapon instead of re-randomizing on death.
    //  Model is kept from initial spawn — no re-randomization.
    Event event(EV_Player_PrimaryDMWeapon);
    event.AddString(m_profile.preferredWeapon.length() ? m_profile.preferredWeapon.c_str() : "auto");

    controlledEnt->ProcessEvent(event);
}

// Refactored in OPM (see github issue #8)
//  Damage perception writes only to BotSenses and the belief map. The
//  state machine reads m_senses.damagedBy/damagedFrom each frame to
//  decide whether to transition into attack, seed the initial enemy,
//  or pre-aim toward the threat. No direct rotation/movement writes.
void BotController::Damaged(const Event& ev)
{
    Entity *attacker = ev.GetEntity(1);

    if (!attacker || attacker == controlledEnt) {
        return;
    }

    // Resolve attacker as a valid sentient enemy (if any)
    Sentient *sentAttacker = NULL;
    if (attacker->IsSubclassOfSentient()) {
        sentAttacker = static_cast<Sentient *>(attacker);

        if (!IsValidEnemy(sentAttacker)) {
            return;
        }
    }

    // Remember attacker position - high confidence
    m_memory.Remember(attacker->origin, AI_EVENT_WEAPON_FIRE, 1.0f);

    // Record the hit into the sense layer
    m_senses.damagedFrom = attacker->origin;
    m_senses.damagedTime = level.inttime;
    m_senses.damagedBy   = sentAttacker;

    if (g_bot_debug_reaction->integer) {
        const char *attackerName = "non-sentient";
        if (sentAttacker) {
            attackerName = sentAttacker->IsSubclassOfPlayer()
                             ? static_cast<Player *>(sentAttacker)->client->pers.netname
                             : sentAttacker->targetname.c_str();
        }
        gi.Printf(
            "BOT %s: Damaged by %s -> senses (pos=(%.0f, %.0f, %.0f))\n",
            controlledEnt->client->pers.netname,
            attackerName,
            attacker->origin.x,
            attacker->origin.y,
            attacker->origin.z
        );
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
    m_planner.SetController(this);

    // Added in OPM
    //  Coverage map is lazily initialized from the nav graph on first
    //  MarkVisited call. No explicit init needed here.

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
