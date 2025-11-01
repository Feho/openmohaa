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
// playerbot_command.cpp: Command execution and event handling for bot controller

#include "g_local.h"
#include "playerbot.h"
#include "consoleevent.h"
#include "scriptexception.h"
#include "g_bot.h"

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

void BotController::Spawned(void)
{
    ClearEnemy();
    m_iCuriousTime   = 0;
    m_botCmd.buttons = 0;

    // Added in OPM - Phase 2B Task 2B.4
    //  Load bot profile on spawn if not already loaded
    if (!profile) {
        // Default to balanced profile, could be made configurable per-bot
        LoadProfile("balanced");
    }
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
    ClearEnemy();
    m_iCuriousTime = 0;

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
