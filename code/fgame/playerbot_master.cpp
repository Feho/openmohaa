#include "playerbot.h"
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
// playerbot_master.cpp: Multiplayer bot system.

#include "g_local.h"
#include "actor.h"
#include "playerbot.h"
#include "consoleevent.h"
#include "debuglines.h"
#include "gamecvars.h"
#include "scriptexception.h"
#include "vehicleturret.h"
#include "weaputils.h"

BotManager botManager;

CLASS_DECLARATION(Listener, BotControllerManager, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Listener, BotManager, NULL) {
    {NULL, NULL}
};

void BotManager::Init()
{
    botProfileManager.Init();
    botControllerManager.Init();
    visibilityMatrix.Reset();
    m_bPendingVisibilityBake = false;
}

void BotManager::Cleanup()
{
    botControllerManager.Cleanup();
    visibilityMatrix.Reset();
    m_bPendingVisibilityBake = false;
}

void BotManager::OnPathNodesLoaded()
{
    m_bPendingVisibilityBake = true;
}

void BotManager::EnsureVisibilityMatrixBaked()
{
    if (!m_bPendingVisibilityBake || visibilityMatrix.IsBaked()) {
        return;
    }

    visibilityMatrix.Bake();
    m_bPendingVisibilityBake = false;
}

void BotManager::Frame()
{
    botControllerManager.ThinkControllers();

    // Added in OPM
    //  Debug visualization for the visibility matrix.
    //  Draws lines from the first bot's nearest node to all visible nodes.
    if (g_bot_debug_visibility->integer) {
        EnsureVisibilityMatrixBaked();
    }

    if (g_bot_debug_visibility->integer && visibilityMatrix.IsBaked()) {
        const Container<BotController *>& controllers = botControllerManager.getControllers();
        if (controllers.NumObjects() > 0) {
            Player *player = controllers.ObjectAt(1)->getControlledEntity();
            if (player) {
                visibilityMatrix.DrawDebug(player->origin);
            }
        }
    }
}

void BotManager::BroadcastEvent(Entity *originator, Vector origin, int iType, float radius)
{
    Sentient      *ent;
    Actor         *act;
    Vector         delta;
    str            name;
    float          r2;
    float          dist2;
    int            i;
    int            iNumSentients;
    int            iAreaNum;
    BotController *controller;

    if (radius <= 0.0f) {
        radius = G_AIEventRadius(iType);
    }

    assert(originator);

    // Changed in OPM
    //  Bots have extended hearing range compared to AI actors.
    //  Gunfire and explosions are audible across most of the map,
    //  footsteps and other sounds from a moderate distance.
    float botRadius;
    switch (iType) {
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_EXPLOSION:
        botRadius = Q_max(radius, 8192.0f);
        break;
    case AI_EVENT_GRENADE:
        botRadius = Q_max(radius, 4096.0f);
        break;
    case AI_EVENT_WEAPON_IMPACT:
        // Ignore bullet impacts - they indicate where bullets hit, not shooter position
        return;
    case AI_EVENT_FOOTSTEP:
        botRadius = Q_max(radius, 1500.0f);
        break;
    default:
        botRadius = Q_max(radius, 2048.0f);
        break;
    }

    r2 = Square(botRadius);

    const Container<BotController *>& controllers = getControllerManager().getControllers();
    for (i = 1; i <= controllers.NumObjects(); i++) {
        controller = controllers.ObjectAt(i);
        ent        = controller->getControlledEntity();
        if (!ent || ent == originator || ent->deadflag) {
            continue;
        }

        delta = origin - ent->centroid;

        // dot product returns length squared
        dist2 = Square(delta);

        if (originator) {
            iAreaNum = originator->edict->r.areanum;
        } else {
            iAreaNum = gi.AreaForPoint(origin);
        }

        if (dist2 > r2) {
            continue;
        }

        if (iAreaNum != ent->edict->r.areanum && !gi.AreasConnected(iAreaNum, ent->edict->r.areanum)) {
            continue;
        }

        controller->NoticeEvent(origin, iType, originator, dist2, r2);
    }
}

BotControllerManager& BotManager::getControllerManager()
{
    return botControllerManager;
}
