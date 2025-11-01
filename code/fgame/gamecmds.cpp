/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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

// gamecmds.cpp: Game commands
//

#include "gamecmds.h"
#include "glb_local.h"
#include "camera.h"
#include "viewthing.h"
#include "soundman.h"
#include "navigate.h"
#include "lodthing.h"
#include "player.h"
#include <scriptcompiler.h>
#include "playerbot.h"
#include "consoleevent.h"
#include "g_bot.h"
// Added in OPM - Phase 2B Task 2B.2
//  Include behavior tree YAML loader and manager
#include "bt_yaml_loader.h"
#include "bt_action_registry.h"
#include "bt_manager.h"

typedef struct {
    const char *command;
    qboolean (*func)(gentity_t *ent);
    qboolean allclients;
} consolecmd_t;

typedef struct {
    const char       *prefix;
    SafePtr<Listener> master;
} commandmaster_t;

// Added in OPM - Phase 2B Task 2B.2
//  Forward declarations for BT console commands
qboolean G_BT_LoadCmd(gentity_t *ent);
qboolean G_BT_ReloadCmd(gentity_t *ent);
qboolean G_BT_ListActionsCmd(gentity_t *ent);
qboolean G_BT_ListConditionsCmd(gentity_t *ent);

// Added in OPM - Phase 2B Task 2B.4
//  Forward declarations for bot profile commands
qboolean G_BotSetProfileCmd(gentity_t *ent);
qboolean G_BotListProfilesCmd(gentity_t *ent);
qboolean G_BotBlackboardCmd(gentity_t *ent);
qboolean G_BotReloadProfilesCmd(gentity_t *ent);

consolecmd_t G_ConsoleCmds[] = {
    //   command name       function             available in multiplayer?
    {"say",             G_SayCmd,             qtrue },
    {"eventlist",       G_EventListCmd,       qfalse},
    {"pendingevents",   G_PendingEventsCmd,   qfalse},
    {"eventhelp",       G_EventHelpCmd,       qfalse},
    {"dumpevents",      G_DumpEventsCmd,      qfalse},
    {"classevents",     G_ClassEventsCmd,     qfalse},
    {"dumpclassevents", G_DumpClassEventsCmd, qfalse},
    {"dumpallclasses",  G_DumpAllClassesCmd,  qtrue },
    {"classlist",       G_ClassListCmd,       qfalse},
    {"classtree",       G_ClassTreeCmd,       qfalse},
    {"cam",             G_CameraCmd,          qfalse},
    {"snd",             G_SoundCmd,           qfalse},
    {"showvar",         G_ShowVarCmd,         qfalse},
    {"levelvars",       G_LevelVarsCmd,       qfalse},
    {"gamevars",        G_GameVarsCmd,        qfalse},
    {"script",          G_ScriptCmd,          qfalse},
    // Added in 2.0
    {"reloadmap",       G_ReloadMap,          qfalse},
    // Added in OPM
    //====
    {"compilescript",       G_CompileScript,         qfalse},
    {"addbot",              G_AddBotCommand,         qfalse},
    {"addbotnamed",         G_AddBotNamedCommand,    qfalse},
    {"removebot",           G_RemoveBotCommand,      qfalse},
    {"bot_debug_info",      G_BotDebugInfoCmd,       qfalse},
    {"bot_force_state",     G_BotForceStateCmd,      qfalse},
    {"bot_show_perception", G_BotShowPerceptionCmd,  qfalse},
#ifdef _DEBUG
    {"bot",                 G_BotCommand,            qfalse},
#endif
    // Added in OPM - Phase 2B Task 2B.4
    //  Bot profile management commands
    {"bot_setprofile",      G_BotSetProfileCmd,      qfalse},
    {"bot_listprofiles",    G_BotListProfilesCmd,    qfalse},
    {"bot_blackboard",      G_BotBlackboardCmd,      qfalse},
    {"bot_reload_profiles", G_BotReloadProfilesCmd,  qfalse},
    // Added in OPM - Phase 2B Task 2B.2
    //  Behavior tree YAML loading commands
    {"bt_load",             G_BT_LoadCmd,            qfalse},
    {"bt_reload",           G_BT_ReloadCmd,          qfalse},
    {"bt_unload",           G_BT_UnloadCmd,          qfalse},
    {"bt_list",             G_BT_ListTreesCmd,       qfalse},
    {"bt_list_actions",     G_BT_ListActionsCmd,     qfalse},
    {"bt_list_conditions",  G_BT_ListConditionsCmd,  qfalse},
    //====
    {NULL,                  NULL,                    qfalse}
};

Container<commandmaster_t> commandMasters;

void G_InitConsoleCommands(void)
{
    consolecmd_t *cmds;

    //
    // the game server will interpret these commands, which will be automatically
    // forwarded to the server after they are not recognized locally
    //
    gi.AddCommand("give", NULL);
    gi.AddCommand("god", NULL);
    gi.AddCommand("notarget", NULL);
    gi.AddCommand("noclip", NULL);
    gi.AddCommand("kill", NULL);
    gi.AddCommand("script", NULL);
    gi.AddCommand("ready", NULL);
    gi.AddCommand("notready", NULL);
    gi.AddCommand("invprev", NULL);
    gi.AddCommand("invnext", NULL);
    gi.AddCommand("weapprev", NULL);
    gi.AddCommand("weapnext", NULL);
    gi.AddCommand("reload", NULL);
    gi.AddCommand("gameversion", NULL);
    gi.AddCommand("fov", NULL);
    gi.AddCommand("holster", NULL);
    gi.AddCommand("safeholster", NULL);
    gi.AddCommand("safezoom", NULL);
    gi.AddCommand("zoomoff", NULL);
    gi.AddCommand("join_team", NULL);
    gi.AddCommand("spectator", NULL);
    gi.AddCommand("primarydmweapon", NULL);
    gi.AddCommand("secondarydmweapon", NULL);
    gi.AddCommand("dmmessage", NULL);

    for (cmds = G_ConsoleCmds; cmds->command != NULL; cmds++) {
        gi.AddCommand(cmds->command, NULL);
    }
}

qboolean G_ConsoleCommand(void)
{
    gentity_t    *ent;
    qboolean      result;
    consolecmd_t *cmds;
    const char   *cmd;

    result = qfalse;
    try {
        ent = &g_entities[0];

        cmd = gi.Argv(0);

        for (cmds = G_ConsoleCmds; cmds->command != NULL; cmds++) {
            if (!Q_stricmp(cmd, cmds->command)) {
                return cmds->func(ent);
            }
        }

        if (cl_running->integer) {
            // Don't execute the command in multiplayer
            // otherwise, the command will be executed "by" the first client
            result = G_ProcessClientCommand(ent);
        }
    } catch (const char *error) {
        G_ExitWithError(error);
    }

    return result;
}

void G_ClientCommand(gentity_t *ent)
{
    try {
        if (ent && !G_ProcessClientCommand(ent)) {
            // anything that doesn't match a command will be a chat
            //G_Say( ent, false, true );
        }
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

qboolean G_ProcessClientCommand(gentity_t *ent)
{
    const char   *cmd;
    consolecmd_t *cmds;
    int           i;
    int           n;
    Player       *player;
    qboolean      allowDev;

    if (!ent || !ent->client || !ent->entity) {
        // not fully in game yet
        return qfalse;
    }

    // Added in 2.1
    //  Prevent players from messing with the server with developer commands
    allowDev = g_gametype->integer == GT_SINGLE_PLAYER;

    cmd = gi.Argv(0);

    player                = (Player *)ent->entity;
    player->m_lastcommand = cmd;

    for (cmds = G_ConsoleCmds; cmds->command != NULL; cmds++) {
        // if we have multiple clients and this command isn't allowed by multiple clients, skip it
        if ((game.maxclients > 1) && (!cmds->allclients)) {
            continue;
        }

        if (!Q_stricmp(cmd, cmds->command)) {
            return cmds->func(ent);
        }
    }

    if (Event::Exists(cmd)) {
        ConsoleEvent ev(cmd);
        ev.SetConsoleEdict(ent);

        n = gi.Argc();

        for (i = 1; i < n; i++) {
            ev.AddToken(gi.Argv(i));
        }

        if (!Q_stricmpn(cmd, "lod_", 4)) {
            if (!allowDev) {
                return false;
            }
            return LODModel.ProcessEvent(ev);
        } else if (!Q_stricmpn(cmd, "view", 4)) {
            if (!allowDev) {
                return false;
            }
            return Viewmodel.ProcessEvent(ev);
        } else {
            //
            // Added in OPM
            //
            Listener *master = G_FindMaster(cmd);
            if (master) {
                return master->ProcessEvent(ev);
            }
        }

        if (ent->entity->CheckEventFlags(&ev)) {
            return ent->entity->ProcessEvent(ev);
        }
    }

    return qfalse;
}

/*
==================
G_CreateMaster
==================
*/
void G_CreateMaster(const char *prefix, Listener *master)
{
    commandmaster_t commandMaster;

    int i;

    for (i = 1; i <= commandMasters.NumObjects(); i++) {
        const commandmaster_t& commandMaster = commandMasters.ObjectAt(i);
        if (!str::icmp(commandMaster.prefix, prefix)) {
            return;
        }
    }

    commandMaster.prefix = prefix;
    commandMaster.master = master;
    commandMasters.AddObject(commandMaster);
}

/*
==================
G_MasterMatches
==================
*/
bool G_MasterMatches(const commandmaster_t& commandMaster, const char *command)
{
    const char *s1, *s2;

    s2 = commandMaster.prefix;
    for (s1 = command; *s1 && *s2; s1++, s2++) {
        if (tolower(*s1) != tolower(*s2)) {
            return false;
        }
    }

    return *s1 == '_';
}

/*
==================
G_FindMaster
==================
*/
Listener *G_FindMaster(const char *command)
{
    int i;

    for (i = 1; i <= commandMasters.NumObjects(); i++) {
        const commandmaster_t& commandMaster = commandMasters.ObjectAt(i);
        if (G_MasterMatches(commandMaster, command)) {
            return commandMaster.master;
        }
    }

    return NULL;
}

/*
==================
Cmd_Say_f
==================
*/
void G_Say(gentity_t *ent, qboolean team, qboolean arg0)
{
    int         j;
    gentity_t  *other;
    const char *p;
    char        text[2048];

    if (gi.Argc() < 2 && !arg0) {
        return;
    }

    if (!ent->entity) {
        // just in case we're not joined yet.
        team = false;
    }

    if (!DM_FLAG(DF_MODELTEAMS | DF_SKINTEAMS)) {
        team = false;
    }

    if (team) {
        Com_sprintf(text, sizeof(text), "(%s): ", ent->client->pers.netname);
    } else {
        Com_sprintf(text, sizeof(text), "%s: ", ent->client->pers.netname);
    }

    if (arg0) {
        strcat(text, gi.Argv(0));
        strcat(text, " ");
        strcat(text, gi.Args());
    } else {
        p = gi.Args();

        if (*p == '"') {
            p++;
            strcat(text, p);
            text[strlen(text) - 1] = 0;
        } else {
            strcat(text, p);
        }
    }

    // don't let text be too long for malicious reasons
    if (strlen(text) > 150) {
        text[150] = 0;
    }

    strcat(text, "\n");

    if (dedicated->integer) {
        gi.SendServerCommand(0, "print \"%s\"", text);
    }

    for (j = 0; j < game.maxclients; j++) {
        other = &g_entities[j];
        if (!other->inuse || !other->client || !other->entity) {
            continue;
        }

        gi.SendServerCommand(0, "print \"%s\"", text);
    }
}

qboolean G_CameraCmd(gentity_t *ent)
{
    Event      *ev;
    const char *cmd;
    int         i;
    int         n;

    n = gi.Argc();
    if (!n) {
        gi.Printf("Usage: cam [command] [arg 1]...[arg n]\n");
        return qtrue;
    }

    cmd = gi.Argv(1);
    if (Event::Exists(cmd)) {
        ev = new Event(cmd);

        for (i = 2; i < n; i++) {
            ev->AddToken(gi.Argv(i));
        }

        CameraMan.ProcessEvent(ev);
    } else {
        gi.Printf("Unknown camera command '%s'.\n", cmd);
    }

    return qtrue;
}

qboolean G_SoundCmd(gentity_t *ent)
{
    Event      *ev;
    const char *cmd;
    int         i;
    int         n;

    n = gi.Argc();
    if (!n) {
        gi.Printf("Usage: snd [command] [arg 1]...[arg n]\n");
        return qtrue;
    }

    cmd = gi.Argv(1);
    if (Event::Exists(cmd)) {
        ev = new Event(cmd);

        for (i = 2; i < n; i++) {
            ev->AddToken(gi.Argv(i));
        }

        SoundMan.ProcessEvent(ev);
    } else {
        gi.Printf("Unknown sound command '%s'.\n", cmd);
    }

    return qtrue;
}

qboolean G_SayCmd(gentity_t *ent)
{
    G_Say(ent, false, false);

    return qtrue;
}

qboolean G_EventListCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.Argc() > 1) {
        mask = gi.Argv(1);
    }

    Event::ListCommands(mask);

    return qtrue;
}

qboolean G_PendingEventsCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.Argc() > 1) {
        mask = gi.Argv(1);
    }

    Event::PendingEvents(mask);

    return qtrue;
}

qboolean G_EventHelpCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.Argc() > 1) {
        mask = gi.Argv(1);
    }

    Event::ListDocumentation(mask, false);

    return qtrue;
}

qboolean G_DumpEventsCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.Argc() > 1) {
        mask = gi.Argv(1);
    }

    Event::ListDocumentation(mask, true);

    return qtrue;
}

qboolean G_ClassEventsCmd(gentity_t *ent)
{
    const char *className;

    className = NULL;
    if (gi.Argc() < 2) {
        gi.Printf("Usage: classevents [className]\n");
        className = gi.Argv(1);
    } else {
        className = gi.Argv(1);
        ClassEvents(className, qfalse);
    }
    return qtrue;
}

qboolean G_DumpClassEventsCmd(gentity_t *ent)
{
    const char *className;

    className = NULL;
    if (gi.Argc() < 2) {
        gi.Printf("Usage: dumpclassevents [className]\n");
        className = gi.Argv(1);
    } else {
        className = gi.Argv(1);
        ClassEvents(className, qtrue);
    }
    return qtrue;
}

qboolean G_DumpAllClassesCmd(gentity_t *ent)
{
    DumpAllClasses();
    return qtrue;
}

qboolean G_ClassListCmd(gentity_t *ent)
{
    listAllClasses();

    return qtrue;
}

qboolean G_ClassTreeCmd(gentity_t *ent)
{
    if (gi.Argc() > 1) {
        listInheritanceOrder(gi.Argv(1));
    } else {
        gi.SendServerCommand(ent - g_entities, "print \"Syntax: classtree [classname].\n\"");
    }

    return qtrue;
}

qboolean G_ShowVarCmd(gentity_t *ent)
{
    return qtrue;
}

void PrintVariableList(ScriptVariableList *list) {}

qboolean G_LevelVarsCmd(gentity_t *ent)
{
    gi.Printf("Level Variables\n");
    PrintVariableList(level.vars);

    return qtrue;
}

qboolean G_GameVarsCmd(gentity_t *ent)
{
    gi.Printf("Game Variables\n");
    PrintVariableList(game.vars);

    return qtrue;
}

qboolean G_ScriptCmd(gentity_t *ent)
{
    const char   *script;
    const char   *name;
    Entity       *scriptEnt;
    ConsoleEvent *event;
    int           numArgs;
    int           i;

    numArgs = gi.Argc();

    if (numArgs <= 1) {
        gi.Printf("Usage: script [filename]\n");
        return qtrue;
    }

    if (!sv_cheats->integer) {
        gi.Printf("command not available\n");
        return qtrue;
    }

    script = gi.Argv(1);
    if (*script == '*') {
        scriptEnt = static_cast<Entity *>(G_GetEntity(atoi(script + 1)));
    } else {
        scriptEnt = static_cast<Entity *>(G_FindTarget(0, script));
    }

    if (!scriptEnt) {
        gi.Printf("Could not find entity %s\n", script);
        return qtrue;
    }

    name = gi.Argv(2);
    if (!Event::Exists(name)) {
        gi.Printf("Unknown command '%s'.\n", name);
        return qtrue;
    }

    event = new ConsoleEvent(name);
    event->SetConsoleEdict(ent);

    for (i = 3; i < numArgs; i++) {
        event->AddToken(gi.Argv(i));
    }

    return scriptEnt->ProcessEvent(event);
}

qboolean G_ReloadMap(gentity_t *ent)
{
    char name[256];

    Com_sprintf(name, sizeof(name), "gamemap \"%s\"\n", level.mapname.c_str());
    gi.SendConsoleCommand(name);

    return qtrue;
}

qboolean G_CompileScript(gentity_t *ent)
{
    if (gi.Argc() <= 2) {
        gi.Printf("Usage: compilescript [filename] [output file]\n");
        return qfalse;
    }

    CompileAssemble(gi.Argv(1), gi.Argv(2));
    return qtrue;
}

qboolean G_AddBotCommand(gentity_t *ent)
{
    unsigned int numbots;
    unsigned int totalnumbots;

    if (gi.Argc() <= 1) {
        gi.Printf("Usage: addbot [numbots]\n");
        return qfalse;
    }

    numbots = atoi(gi.Argv(1));

    if (numbots > game.maxclients) {
        gi.Printf("addbot must be between 1-%d\n", game.maxclients);
        return qfalse;
    }

    totalnumbots = Q_min(numbots + sv_numbots->integer, sv_maxbots->integer);

    gi.cvar_set("sv_numbots", va("%d", totalnumbots));
    return qtrue;
}

qboolean G_AddBotNamedCommand(gentity_t *ent)
{
    unsigned int numbots;
    unsigned int totalnumbots;
    const char* name;
    gentity_t *e;

    if (gi.Argc() <= 1) {
        gi.Printf("Usage: addbotnamed [botname]\n");
        return qfalse;
    }

    name = gi.Argv(1);

    totalnumbots = Q_min(sv_numbots->integer + 1, sv_maxbots->integer);

    gi.cvar_set("sv_numbots", va("%d", totalnumbots));

    bot_info_t botInfo;
    botInfo.name = name;

    e = G_AddBot(&botInfo);
    if (e) {
        const unsigned int id = G_GetBotId(e);
        gi.cvar_set(va("sv_bot%dname", id), e->client->pers.netname);
    }

    return qtrue;
}

qboolean G_RemoveBotCommand(gentity_t *ent)
{
    unsigned int numbots;
    unsigned int totalnumbots;

    if (gi.Argc() <= 1) {
        gi.Printf("Usage: removebot [numbots]\n");
        return qfalse;
    }

    numbots      = atoi(gi.Argv(1));
    totalnumbots = sv_numbots->integer - Q_min(numbots, sv_numbots->integer);

    gi.cvar_set("sv_numbots", va("%d", totalnumbots));
    return qtrue;
}

#ifdef _DEBUG

qboolean G_BotCommand(gentity_t *ent)
{
    const char    *command;
    BotController *bot;

    if (botManager.getControllerManager().getControllers().NumObjects() < 1) {
        gi.Printf("No bot spawned.\n");
        return qfalse;
    }

    if (gi.Argc() <= 1) {
        gi.Printf("Usage: bot [cmd] (arg1) (arg2) (arg3) ...\n");
        return qfalse;
    }

    bot = botManager.getControllerManager().getControllers().ObjectAt(1);

    command = gi.Argv(1);

    if (!Q_stricmp(command, "movehere")) {
        bot->GetMovement().MoveTo(ent->entity->origin);
    } else if (!Q_stricmp(command, "moveherenear")) {
        float rad = 256.0f;

        if (gi.Argc() > 2) {
            rad = atof(gi.Argv(2));
        }

        bot->GetMovement().MoveNear(ent->entity->origin, rad);
    } else if (!Q_stricmp(command, "avoidhere")) {
        float rad = 256.0f;

        if (gi.Argc() > 2) {
            rad = atof(gi.Argv(2));
        }

        bot->GetMovement().AvoidPath(ent->entity->origin, rad);
    } else if (!Q_stricmp(command, "telehere")) {
        bot->getControlledEntity()->setOrigin(ent->s.origin);
    }

    return qtrue;
}

#endif

// Added in OPM
//  Bot debug visualization commands

/*
====================
G_BotDebugInfoCmd

Print detailed debug information about a specific bot
Usage: bot_debug_info [botIndex]
====================
*/
qboolean G_BotDebugInfoCmd(gentity_t *ent)
{
    // Changed in OPM
    //  Use bot index instead of client number
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();

    if (controllers.NumObjects() < 1) {
        gi.Printf("No bots spawned\n");
        return qfalse;
    }

    // Get bot index from argument (default to 1 for first bot)
    int botIndex = 1;
    if (gi.Argc() >= 2) {
        botIndex = atoi(gi.Argv(1));
    }

    // Validate bot index (Container uses 1-based indexing)
    if (botIndex < 1 || botIndex > controllers.NumObjects()) {
        gi.Printf("Invalid bot index %d (valid range: 1-%d)\n", botIndex, controllers.NumObjects());
        gi.Printf("Usage: bot_debug_info [botIndex]\n");
        return qfalse;
    }

    // Get the controller
    BotController *bot = controllers.ObjectAt(botIndex);
    if (!bot) {
        gi.Printf("Failed to get bot %d controller\n", botIndex);
        return qfalse;
    }

    // Call debug method
    bot->PrintDebugInfo();
    gi.Printf("Debug info for bot %d printed\n", botIndex);
    return qtrue;
}

/*
====================
G_BotForceStateCmd

Force a bot into a specific state for testing
Usage: bot_force_state <botIndex> <stateIndex>
State indices: 0=Attack, 1=Investigate, 2=Curious, 3=Grenade, 4=Idle
====================
*/
qboolean G_BotForceStateCmd(gentity_t *ent)
{
    // Changed in OPM
    //  Use bot index instead of client number
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();

    if (controllers.NumObjects() < 1) {
        gi.Printf("No bots spawned\n");
        return qfalse;
    }

    if (gi.Argc() < 3) {
        gi.Printf("Usage: bot_force_state <botIndex> <stateIndex>\n");
        gi.Printf("State indices: 0=Attack, 1=Investigate, 2=Curious, 3=Grenade, 4=Idle\n");
        return qfalse;
    }

    int botIndex   = atoi(gi.Argv(1));
    int stateIndex = atoi(gi.Argv(2));

    // Validate bot index (Container uses 1-based indexing)
    if (botIndex < 1 || botIndex > controllers.NumObjects()) {
        gi.Printf("Invalid bot index %d (valid range: 1-%d)\n", botIndex, controllers.NumObjects());
        return qfalse;
    }

    // Get the controller
    BotController *bot = controllers.ObjectAt(botIndex);
    if (!bot) {
        gi.Printf("Failed to get bot %d controller\n", botIndex);
        return qfalse;
    }

    // Force the state
    bot->ForceState(stateIndex);
    gi.Printf("Bot %d forced to state %d\n", botIndex, stateIndex);
    return qtrue;
}

/*
====================
G_BotShowPerceptionCmd

Toggle perception visualization for a specific bot
Usage: bot_show_perception [botIndex]
====================
*/
qboolean G_BotShowPerceptionCmd(gentity_t *ent)
{
    // Changed in OPM
    //  Use bot index instead of client number
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();

    if (controllers.NumObjects() < 1) {
        gi.Printf("No bots spawned\n");
        return qfalse;
    }

    // Get bot index from argument (default to 1 for first bot)
    int botIndex = 1;
    if (gi.Argc() >= 2) {
        botIndex = atoi(gi.Argv(1));
    }

    // Validate bot index (Container uses 1-based indexing)
    if (botIndex < 1 || botIndex > controllers.NumObjects()) {
        gi.Printf("Invalid bot index %d (valid range: 1-%d)\n", botIndex, controllers.NumObjects());
        gi.Printf("Usage: bot_show_perception [botIndex]\n");
        return qfalse;
    }

    // Get the controller
    BotController *bot = controllers.ObjectAt(botIndex);
    if (!bot) {
        gi.Printf("Failed to get bot %d controller\n", botIndex);
        return qfalse;
    }

    // Toggle perception visualization
    bot->TogglePerceptionVisualization();
    gi.Printf("Bot %d perception visualization toggled\n", botIndex);
    return qtrue;
}

// Added in OPM - Phase 2B Task 2B.2
//  Console commands for behavior tree YAML loading
qboolean G_BT_LoadCmd(gentity_t *ent)
{
    if (gi.Argc() < 2) {
        gi.Printf("Usage: bt_load <filename>\n");
        gi.Printf("Example: bt_load engage_enemy\n");
        return qfalse;
    }

    const char *filename = gi.Argv(1);

    // Changed in OPM
    //  Added path traversal protection to prevent directory escape attacks
    // Validate filename to prevent path traversal attacks
    if (!filename || strlen(filename) == 0) {
        gi.Printf("ERROR: Empty filename\n");
        return qfalse;
    }

    // Check for path traversal sequences
    if (strstr(filename, "..") != nullptr || strchr(filename, '/') != nullptr || strchr(filename, '\\') != nullptr) {
        gi.Printf("ERROR: Invalid filename '%s' - path separators and '..' not allowed\n", filename);
        return qfalse;
    }

    // Check filename length (leave room for path prefix and extension)
    constexpr size_t maxFilenameLen = 200;
    if (strlen(filename) > maxFilenameLen) {
        gi.Printf("ERROR: Filename too long (max %d characters)\n", static_cast<int>(maxFilenameLen));
        return qfalse;
    }

    char path[256];
    Com_sprintf(path, sizeof(path), "behaviors/%s.yaml", filename);

    // Changed in OPM
    //  Use BTManager to load and store tree
    if (BTManager::Instance().LoadTree(filename, path)) {
        gi.Printf("Successfully loaded behavior tree: %s\n", filename);
        gi.Printf("  Use 'bt_list' to see all loaded trees\n");
        return qtrue;
    } else {
        gi.Printf("Failed to load behavior tree: %s\n", filename);
        return qfalse;
    }
}

qboolean G_BT_ReloadCmd(gentity_t *ent)
{
    if (gi.Argc() < 2) {
        gi.Printf("Usage: bt_reload <tree_name>\n");
        gi.Printf("Example: bt_reload engage_enemy\n");
        return qfalse;
    }

    const char *treeName = gi.Argv(1);

    // Changed in OPM
    //  Implement proper reload semantics using BTManager
    if (!BTManager::Instance().HasTree(treeName)) {
        gi.Printf("ERROR: Tree '%s' not loaded. Use bt_load to load it first.\n", treeName);
        gi.Printf("  Use 'bt_list' to see all loaded trees\n");
        return qfalse;
    }

    if (BTManager::Instance().ReloadTree(treeName)) {
        gi.Printf("Successfully reloaded behavior tree: %s\n", treeName);
        return qtrue;
    } else {
        gi.Printf("Failed to reload behavior tree: %s\n", treeName);
        return qfalse;
    }
}

qboolean G_BT_ListActionsCmd(gentity_t *ent)
{
    auto actions = BTActionRegistry::Instance().GetActionNames();

    gi.Printf("=== Registered BT Actions (%d) ===\n", static_cast<int>(actions.size()));
    for (const auto &name : actions) {
        gi.Printf("  - %s\n", name.c_str());
    }

    return qtrue;
}

qboolean G_BT_ListConditionsCmd(gentity_t *ent)
{
    auto conditions = BTActionRegistry::Instance().GetConditionNames();

    gi.Printf("=== Registered BT Conditions (%d) ===\n", static_cast<int>(conditions.size()));
    for (const auto &name : conditions) {
        gi.Printf("  - %s\n", name.c_str());
    }

    return qtrue;
}

// Added in OPM - Phase 2B Task 2B.2 Review Fixes
//  New command to list loaded behavior trees
qboolean G_BT_ListTreesCmd(gentity_t *ent)
{
    auto treeNames = BTManager::Instance().GetLoadedTreeNames();

    gi.Printf("=== Loaded Behavior Trees (%d) ===\n", static_cast<int>(treeNames.size()));
    if (treeNames.empty()) {
        gi.Printf("  (none loaded - use 'bt_load' to load trees)\n");
    } else {
        for (const auto &name : treeNames) {
            gi.Printf("  - %s\n", name.c_str());
        }
    }

    return qtrue;
}

// Added in OPM - Phase 2B Task 2B.2 Review Fixes
//  New command to unload a behavior tree
qboolean G_BT_UnloadCmd(gentity_t *ent)
{
    if (gi.Argc() < 2) {
        gi.Printf("Usage: bt_unload <tree_name>\n");
        gi.Printf("Example: bt_unload engage_enemy\n");
        return qfalse;
    }

    const char *treeName = gi.Argv(1);

    if (BTManager::Instance().UnloadTree(treeName)) {
        gi.Printf("Unloaded behavior tree: %s\n", treeName);
        return qtrue;
    } else {
        gi.Printf("ERROR: Tree '%s' not found\n", treeName);
        gi.Printf("  Use 'bt_list' to see all loaded trees\n");
        return qfalse;
    }
}

// Added in OPM - Phase 2B Task 2B.4
//  Bot profile management commands

/*
====================
G_BotSetProfileCmd

Change a bot's active profile at runtime
Usage: bot_setprofile <botIndex> <profileName>
====================
*/
qboolean G_BotSetProfileCmd(gentity_t *ent)
{
    if (gi.Argc() < 3) {
        gi.Printf("Usage: bot_setprofile <botIndex> <profileName>\n");
        gi.Printf("Example: bot_setprofile 1 aggressive\n");
        gi.Printf("Available profiles: aggressive, balanced, defensive, sniper, rusher\n");
        return qfalse;
    }

    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();

    if (controllers.NumObjects() < 1) {
        gi.Printf("No bots spawned\n");
        return qfalse;
    }

    int         botIndex    = atoi(gi.Argv(1));
    const char *profileName = gi.Argv(2);

    // Validate bot index (Container uses 1-based indexing)
    if (botIndex < 1 || botIndex > controllers.NumObjects()) {
        gi.Printf("Invalid bot index %d (valid range: 1-%d)\n", botIndex, controllers.NumObjects());
        return qfalse;
    }

    // Get the controller
    BotController *bot = controllers.ObjectAt(botIndex);
    if (!bot) {
        gi.Printf("Failed to get bot %d controller\n", botIndex);
        return qfalse;
    }

    // Load the new profile
    bot->LoadProfile(profileName);
    gi.Printf("Bot %d profile set to '%s'\n", botIndex, profileName);

    return qtrue;
}

/*
====================
G_BotListProfilesCmd

List all available bot profiles
Usage: bot_listprofiles
====================
*/
qboolean G_BotListProfilesCmd(gentity_t *ent)
{
    gi.Printf("=== Available Bot Profiles ===\n");

    // Hardcoded list of standard profiles (Task 2B.3)
    const char *profiles[] = {"aggressive", "balanced", "defensive", "sniper", "rusher"};

    for (int i = 0; i < 5; i++) {
        gi.Printf("  - %s\n", profiles[i]);
    }

    gi.Printf("\nUsage: bot_setprofile <botIndex> <profileName>\n");
    gi.Printf("Example: bot_setprofile 1 aggressive\n");

    return qtrue;
}

/*
====================
G_BotBlackboardCmd

Display blackboard contents for a specific bot
Usage: bot_blackboard <botIndex>
====================
*/
qboolean G_BotBlackboardCmd(gentity_t *ent)
{
    if (gi.Argc() < 2) {
        gi.Printf("Usage: bot_blackboard <botIndex>\n");
        gi.Printf("Example: bot_blackboard 1\n");
        return qfalse;
    }

    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();

    if (controllers.NumObjects() < 1) {
        gi.Printf("No bots spawned\n");
        return qfalse;
    }

    int botIndex = atoi(gi.Argv(1));

    // Validate bot index (Container uses 1-based indexing)
    if (botIndex < 1 || botIndex > controllers.NumObjects()) {
        gi.Printf("Invalid bot index %d (valid range: 1-%d)\n", botIndex, controllers.NumObjects());
        return qfalse;
    }

    // Get the controller
    BotController *bot = controllers.ObjectAt(botIndex);
    if (!bot) {
        gi.Printf("Failed to get bot %d controller\n", botIndex);
        return qfalse;
    }

    // Get blackboard reference
    Blackboard &bb = bot->GetBlackboard();

    gi.Printf("=== Bot %d Blackboard ===\n", botIndex);

    // Display health information
    if (bb.Has("health")) {
        float health    = bb.GetOrDefault<float>("health", 0.0f);
        float maxHealth = bb.GetOrDefault<float>("maxHealth", 100.0f);
        gi.Printf("  health: %.1f / %.1f (%.0f%%)\n", health, maxHealth, (health / maxHealth) * 100.0f);
    }

    // Display enemy information
    if (bb.Has("enemy")) {
        auto enemy = bb.TryGet<Sentient *>("enemy");
        if (enemy && *enemy) {
            gi.Printf("  enemy: %s (entnum %d)\n", (*enemy)->targetname.c_str(), (*enemy)->entnum);
        } else {
            gi.Printf("  enemy: (none)\n");
        }
    }

    // Display profile information
    if (bb.Has("profile")) {
        auto profile = bb.TryGet<BotProfile *>("profile");
        if (profile && *profile) {
            gi.Printf("  profile: %s\n", (*profile)->GetName().c_str());
            gi.Printf("  behaviorTree: %s\n", (*profile)->GetBehaviorTree().c_str());
        }
    }

    gi.Printf("  (Use g_bot_debug %d for more detailed information)\n", botIndex);

    return qtrue;
}

/*
====================
G_BotReloadProfilesCmd

Reload profiles for all bots from disk
Usage: bot_reload_profiles
====================
*/
qboolean G_BotReloadProfilesCmd(gentity_t *ent)
{
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();

    if (controllers.NumObjects() < 1) {
        gi.Printf("No bots spawned\n");
        return qfalse;
    }

    gi.Printf("Reloading all bot profiles...\n");

    int reloadCount = 0;
    for (int i = 1; i <= controllers.NumObjects(); i++) {
        BotController *bot = controllers.ObjectAt(i);
        if (bot) {
            bot->ReloadProfile();
            reloadCount++;
        }
    }

    gi.Printf("Reloaded profiles for %d bots\n", reloadCount);

    return qtrue;
}
