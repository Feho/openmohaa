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
// playerbot_util.cpp: Utility and helper functions for bot controller

#include "g_local.h"
#include "playerbot.h"
#include "vehicleturret.h"
#include "weaputils.h"
#include "windows.h"
#include "debuglines.h"

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
GetEventPriority

Get priority level for event type
Returns: 0=none, 1=low priority (Curious state), 2=high priority (Investigation state)
====================
*/
int BotController::GetEventPriority(int eventType)
{
    switch (eventType) {
    // High priority events - trigger Investigation state
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_WEAPON_IMPACT:
    case AI_EVENT_EXPLOSION:
    case AI_EVENT_GRENADE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
        return 2;

    // Low priority events - trigger Curious state
    case AI_EVENT_FOOTSTEP:
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
        return 1;

    // No priority
    case AI_EVENT_NONE:
    case AI_EVENT_BADPLACE:
    default:
        return 0;
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

    // Determine event priority level
    int newEventPriority = GetEventPriority(iType);
    if (newEventPriority == 0) {
        // Not a relevant event type
        return;
    }

    // Random chance based on distance (closer events more likely to be noticed)
    fRangeFactor = 1.0 - (fDistanceSquared / fRadiusSquared);
    if (fRangeFactor < random()) {
        return;
    }

    // Get event owner to filter out teammates
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

    // If in Attack state, ignore all sound events (already engaged with visible enemy)
    if (m_pEnemy) {
        return;
    }

    // Priority-based event handling
    bool shouldAcceptEvent = false;

    if (memoryState.currentEventPriority == 0) {
        // No active investigation, always accept
        shouldAcceptEvent = true;
    } else if (newEventPriority > memoryState.currentEventPriority) {
        // Higher priority event interrupts lower priority investigation
        shouldAcceptEvent = true;
    } else if (newEventPriority == memoryState.currentEventPriority) {
        // Same priority - compare distances
        Vector currentInvestigatePos = (memoryState.currentEventPriority == 2) ? memoryState.investigateEventPos : m_vNewCuriousPos;
        Vector deltaNew     = vPos - controlledEnt->origin;
        Vector deltaCurrent = currentInvestigatePos - controlledEnt->origin;

        if (deltaNew.lengthSquared() < deltaCurrent.lengthSquared()) {
            // New event is closer, accept it
            shouldAcceptEvent = true;
        }
    }

    if (!shouldAcceptEvent) {
        // Ignore this event
        return;
    }

    // Accept the event and set appropriate state variables based on priority
    memoryState.currentEventPriority = newEventPriority;

    if (newEventPriority == 2) {
        // High priority event - trigger Investigation state
        memoryState.investigateEventTime = level.svsTime;
        memoryState.investigateEventPos  = vPos;

        if (g_bot_debug->integer >= 1) {
            const char* eventName = G_AIEventStringFromType(iType);
            gi.Printf("[BOT] %s: High-priority sound event '%s' at distance %.0f - investigating\n",
                controlledEnt->client->pers.netname, eventName, sqrt(fDistanceSquared));
        }
    } else {
        // Low priority event - trigger Curious state
        m_iCuriousTime   = level.inttime + 20000;
        m_vNewCuriousPos = vPos;

        if (g_bot_debug->integer >= 2) {
            const char* eventName = G_AIEventStringFromType(iType);
            gi.Printf("[BOT] %s: Low-priority sound event '%s' at distance %.0f - curious\n",
                controlledEnt->client->pers.netname, eventName, sqrt(fDistanceSquared));
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
    m_iAttackTime   = 0;
    m_pEnemy        = NULL;
    m_iEnemyEyesTag = -1;
    m_vOldEnemyPos  = vec_zero;
    m_vLastEnemyPos = vec_zero;
}

// Added in OPM
//  Debug visualization and introspection methods

/*
====================
PrintDebugInfo

Print detailed debug information about the bot
====================
*/
void BotController::PrintDebugInfo(void)
{
    if (!controlledEnt) {
        gi.Printf("Bot controller has no controlled entity\n");
        return;
    }

    Player *player = (Player *)controlledEnt.Pointer();

    gi.Printf("=== Bot Debug Info: %s ===\n", player->client->pers.netname);
    gi.Printf("Client Number: %d\n", player->client->ps.clientNum);
    gi.Printf("Origin: %.1f %.1f %.1f\n", player->origin[0], player->origin[1], player->origin[2]);
    gi.Printf("Health: %.0f / %.0f\n", player->health, player->max_health);

    // Current state information
    gi.Printf("\n--- Active States ---\n");
    const char *stateNames[] = {"Attack", "Investigate", "Curious", "Grenade", "Idle"};
    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        if (m_StateFlags & (1 << i)) {
            int duration = (level.inttime - m_iStateEntryTime[i]) / 1000;
            gi.Printf("  [%d] %s (active for %ds)\n", i, stateNames[i], duration);
        }
    }

    // Enemy information
    gi.Printf("\n--- Enemy Info ---\n");
    if (m_pEnemy) {
        Sentient *enemy = (Sentient *)m_pEnemy.Pointer();
        if (enemy) {
            Vector delta = enemy->origin - player->origin;
            float  dist  = delta.length();
            gi.Printf("  Current Enemy: %s (entnum %d)\n", enemy->targetname.c_str(), enemy->entnum);
            gi.Printf("  Distance: %.1f units\n", dist);
            gi.Printf("  Last Seen: %.1fs ago\n", (level.inttime - m_iLastSeenTime) / 1000.0f);
        }
    } else if (memoryState.enemyMemory.enemy) {
        gi.Printf("  Enemy Memory (lost):\n");
        gi.Printf("    Last Known Position: %.1f %.1f %.1f\n",
                  memoryState.enemyMemory.lastKnownPosition[0],
                  memoryState.enemyMemory.lastKnownPosition[1],
                  memoryState.enemyMemory.lastKnownPosition[2]);
        gi.Printf("    Last Seen: %.1fs ago\n", (level.inttime - (int)(memoryState.enemyMemory.lastSeenTime * 1000)) / 1000.0f);
        gi.Printf("    Confidence: %.2f\n", memoryState.enemyMemory.confidenceLevel);
    } else {
        gi.Printf("  No enemy\n");
    }

    // Weapon information
    gi.Printf("\n--- Weapon Info ---\n");
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (weapon) {
        gi.Printf("  Weapon: %s\n", weapon->getName().c_str());
        gi.Printf("  Ammo in clip: %d\n", weapon->ClipAmmo(FIRE_PRIMARY));
        gi.Printf("  Has ammo: %s\n", weapon->HasAmmoInClip(FIRE_PRIMARY) ? "Yes" : "No");
    } else {
        gi.Printf("  No active weapon\n");
    }

    // Movement information
    gi.Printf("\n--- Movement Info ---\n");
    if (movement.IsMoving()) {
        Vector goal = movement.GetCurrentGoal();
        Vector delta = goal - player->origin;
        float dist = delta.length();
        gi.Printf("  Moving to: %.1f %.1f %.1f\n", goal[0], goal[1], goal[2]);
        gi.Printf("  Distance to goal: %.1f units\n", dist);
    } else {
        gi.Printf("  Not moving\n");
    }

    // Tactical information
    gi.Printf("\n--- Tactical Info ---\n");
    const char *profileNames[] = {"Aggressive", "Cautious", "Defensive", "Retreating"};
    gi.Printf("  Combat Profile: %s\n", profileNames[m_combatProfile]);
    const char *fireModeNames[] = {"Accurate", "Burst", "Suppression", "Melee"};
    gi.Printf("  Fire Mode: %s\n", fireModeNames[m_fireMode]);

    if (coverState.state != COVER_NONE) {
        const char *coverStateNames[] = {"None", "Moving to Cover", "In Cover", "Peeking", "Repositioning"};
        gi.Printf("  Cover State: %s\n", coverStateNames[coverState.state]);
        gi.Printf("  Cover Quality: %.2f\n", coverState.current.quality);
    }

    // Squad information
    if (squadState.squad.members.NumObjects() > 1) {
        gi.Printf("\n--- Squad Info ---\n");
        const char *roleNames[] = {"None", "Aggressor", "Flanker", "Support", "Defender"};
        gi.Printf("  Squad Role: %s\n", roleNames[squadState.role]);
        gi.Printf("  Squad Size: %d bots\n", squadState.squad.members.NumObjects());
        if (squadState.squad.sharedTarget) {
            gi.Printf("  Shared Target: %s\n", squadState.squad.sharedTarget->targetname.c_str());
        }
    }

    // Timers
    gi.Printf("\n--- Timers ---\n");
    if (m_iCuriousTime > level.inttime) {
        gi.Printf("  Curious expires in: %.1fs\n", (m_iCuriousTime - level.inttime) / 1000.0f);
    }
    if (memoryState.investigateEventTime > 0) {
        int elapsed = (level.svsTime - memoryState.investigateEventTime) / 1000;
        gi.Printf("  Investigating for: %ds\n", elapsed);
    }

    gi.Printf("=========================\n");
}

/*
====================
ForceState

Force the bot into a specific state for testing
====================
*/
void BotController::ForceState(int stateIndex)
{
    if (stateIndex < 0 || stateIndex >= MAX_BOT_FUNCTIONS) {
        gi.Printf("Invalid state index %d (valid range: 0-%d)\n", stateIndex, MAX_BOT_FUNCTIONS - 1);
        return;
    }

    const char *stateNames[] = {"Attack", "Investigate", "Curious", "Grenade", "Idle"};

    // End current states
    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        if (m_StateFlags & (1 << i)) {
            botfunc_t *func = &botfuncs[i];
            if (func->EndState) {
                (this->*func->EndState)();
            }
        }
    }

    // Clear all state flags
    m_StateFlags = 0;

    // Set new state
    m_StateFlags |= (1 << stateIndex);
    m_iStateEntryTime[stateIndex] = level.inttime;

    // Call begin state
    botfunc_t *func = &botfuncs[stateIndex];
    if (func->BeginState) {
        (this->*func->BeginState)();
    }

    gi.Printf("Forced bot '%s' into state [%d] %s\n",
              controlledEnt->client->pers.netname, stateIndex, stateNames[stateIndex]);
}

/*
====================
TogglePerceptionVisualization

Toggle perception visualization for this bot
====================
*/
void BotController::TogglePerceptionVisualization(void)
{
    m_bShowPerception = !m_bShowPerception;
    m_bShowPath       = !m_bShowPath;
    m_bShowEnemy      = !m_bShowEnemy;
    m_bShowState      = !m_bShowState;

    if (m_bShowPerception) {
        gi.Printf("Enabled perception visualization for bot '%s'\n", controlledEnt->client->pers.netname);
    } else {
        gi.Printf("Disabled perception visualization for bot '%s'\n", controlledEnt->client->pers.netname);
    }
}

/*
====================
DrawDebugVisualization

Draw debug visualization for the bot (called each frame)
====================
*/
void BotController::DrawDebugVisualization(void)
{
    if (!controlledEnt) {
        return;
    }

    Player *player = (Player *)controlledEnt.Pointer();
    Vector origin  = player->origin + Vector(0, 0, player->viewheight);

    // Draw state information overlay
    if (m_bShowState) {
        const char *stateNames[] = {"Attack", "Investigate", "Curious", "Grenade", "Idle"};
        str stateText = "State: ";
        bool hasState = false;

        for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
            if (m_StateFlags & (1 << i)) {
                if (hasState) {
                    stateText += ", ";
                }
                stateText += stateNames[i];
                hasState = true;
            }
        }

        if (!hasState) {
            stateText += "None";
        }

        // Draw state text above bot
        G_DebugString(origin + Vector(0, 0, 20), 0.5f, 1.0f, 1.0f, 1.0f, stateText.c_str());

        // Draw combat profile
        const char *profileNames[] = {"Aggressive", "Cautious", "Defensive", "Retreating"};
        G_DebugString(origin + Vector(0, 0, 15), 0.4f, 0.8f, 0.8f, 0.8f, "Profile: %s", profileNames[m_combatProfile]);

        // Draw health
        G_DebugString(origin + Vector(0, 0, 10), 0.4f, 0.0f, 1.0f, 0.0f, "HP: %.0f", player->health);
    }

    // Draw enemy visualization
    if (m_bShowEnemy) {
        if (m_pEnemy) {
            Sentient *enemy = (Sentient *)m_pEnemy.Pointer();
            if (enemy) {
                // Draw line to visible enemy (solid green)
                G_DebugLine(origin, enemy->centroid, 0.0f, 1.0f, 0.0f, 1.0f);

                // Draw target lock indicator
                G_DebugCircle(enemy->centroid, 32.0f, 1.0f, 0.0f, 0.0f, 1.0f, qfalse);
            }
        } else if (memoryState.enemyMemory.enemy) {
            // Draw line to remembered enemy position (dashed orange)
            G_LineStipple(4, 0x0F0F);
            G_DebugLine(origin, memoryState.enemyMemory.lastKnownPosition, 1.0f, 0.6f, 0.0f, 0.7f);
            G_LineStipple(1, 0xFFFF);

            // Draw memory indicator
            G_DebugCircle(memoryState.enemyMemory.lastKnownPosition, 24.0f, 1.0f, 0.5f, 0.0f, 0.7f, qfalse);
        }
    }

    // Draw perception visualization
    if (m_bShowPerception) {
        // Draw FOV cone (simplified - just show viewing direction)
        Vector forward;
        player->angles.AngleVectorsLeft(&forward, NULL, NULL);
        Vector endPos = origin + forward * 1000.0f;
        G_DebugLine(origin, endPos, 0.0f, 0.5f, 1.0f, 0.3f);

        // Draw audio radius (simplified circle)
        G_DebugCircle(player->origin, SOUND_RADIUS, 0.5f, 0.5f, 0.5f, 0.2f, qtrue);
    }

    // Draw path visualization
    if (m_bShowPath && movement.IsMoving()) {
        Vector goal = movement.GetCurrentGoal();
        Vector dir = movement.GetCurrentPathDirection();

        // Draw line to current goal (red)
        G_DebugLine(origin, goal, 1.0f, 0.0f, 0.0f, 1.0f);

        // Draw goal marker
        G_DebugCircle(goal, 16.0f, 1.0f, 0.0f, 0.0f, 1.0f, qfalse);

        // Draw movement direction arrow
        if (dir.lengthSquared() > 0.01f) {
            Vector normalizedDir = dir;
            normalizedDir.normalize();
            G_DebugArrow(player->origin, normalizedDir, 100.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        }
    }
}
