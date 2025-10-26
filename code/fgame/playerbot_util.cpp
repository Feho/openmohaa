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
