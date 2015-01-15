/**
 * @file
 * @brief Main game functions.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_main.c
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "g_local.h"
#include "g_ai.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_match.h"
#include "g_spawn.h"
#include "g_utils.h"

game_locals_t game;
level_locals_t level;
game_import_t gi;
game_export_t globals;

#ifndef HARD_LINKED_GAME
cvar_t* sv_maxclients;
cvar_t* sv_dedicated;
cvar_t* developer;
#endif

cvar_t* logstats;
FILE* logstatsfile;

cvar_t* sv_needpass;
cvar_t* sv_maxplayersperteam;
cvar_t* sv_maxsoldiersperteam;
cvar_t* sv_maxsoldiersperplayer;
cvar_t* sv_enablemorale;
cvar_t* sv_roundtimelimit;
cvar_t* sv_maxentities;
cvar_t* sv_filterban;
cvar_t* sv_maxteams;
cvar_t* sv_ai;
cvar_t* sv_teamplay;
cvar_t* sv_hurtaliens;
cvar_t* sv_shot_origin;
static cvar_t* sv_cheats;
static cvar_t* sv_send_edicts;

cvar_t* password;

cvar_t* ai_alienteam;
cvar_t* ai_civilianteam;
cvar_t* ai_equipment;
cvar_t* ai_singleplayeraliens;
cvar_t* ai_numcivilians;
cvar_t* ai_multiplayeraliens;

/* morale cvars */
cvar_t* mob_death;
cvar_t* mob_wound;
cvar_t* mof_watching;
cvar_t* mof_teamkill;
cvar_t* mof_civilian;
cvar_t* mof_enemy;
cvar_t* mor_pain;

/*everyone gets this times morale damage */
cvar_t* mor_default;

/*at this distance the following two get halfed (exponential scale) */
cvar_t* mor_distance;

/*at this distance the following two get halfed (exponential scale) */
cvar_t* mor_victim;

/*at this distance the following two get halfed (exponential scale) */
cvar_t* mor_attacker;

/* how much the morale depends on the size of the damaged team */
cvar_t* mon_teamfactor;

cvar_t* mor_regeneration;
cvar_t* mor_shaken;
cvar_t* mor_panic;
cvar_t* mor_brave;

cvar_t* m_sanity;
cvar_t* m_rage;
cvar_t* m_rage_stop;
cvar_t* m_panic_stop;

cvar_t* g_endlessaliens;
cvar_t* g_ailua;
cvar_t* g_aihumans;
cvar_t* g_aidebug;
cvar_t* g_drawtraces;
cvar_t* g_nodamage;
cvar_t* g_notu;
cvar_t* g_nospawn;
cvar_t* g_actorspeed;
cvar_t* g_lastseen;
cvar_t* flood_msgs;
cvar_t* flood_persecond;
cvar_t* flood_waitdelay;

cvar_t* g_difficulty;

static const int TAG_INVENTORY = 2389;

static void G_FreeInventory (void* data)
{
	G_MemFree(data);
}

static void* G_AllocInventoryMemory (size_t size)
{
	return G_TagMalloc(size, TAG_INVENTORY);
}

static void G_FreeAllInventory (void)
{
	G_FreeTags(TAG_INVENTORY);
}

static const inventoryImport_t inventoryImport = { G_FreeInventory, G_FreeAllInventory, G_AllocInventoryMemory };


/**
 * @brief This will be called when the game library is first loaded
 * @note only happens when a new game/map is started
 */
static void G_Init (void)
{
	gi.DPrintf("==== InitGame ====\n");

	/* noset vars */
	sv_dedicated = gi.Cvar_GetOrCreate("sv_dedicated", "0", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");

	/* latched vars */
	sv_cheats = gi.Cvar_GetOrCreate("sv_cheats", "0", CVAR_SERVERINFO | CVAR_LATCH, "Activate cheats");
	gi.Cvar_GetOrCreate("gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH, nullptr);
	gi.Cvar_GetOrCreate("gamedate", __DATE__, CVAR_SERVERINFO | CVAR_LATCH, nullptr);
	developer = gi.Cvar_GetOrCreate("developer", "0", 0, "Print out a lot of developer debug messages - useful to track down bugs");
	logstats = gi.Cvar_GetOrCreate("logstats", "1", CVAR_ARCHIVE, "Server logfile output for kills");

	/* max. players per team (original quake) */
	sv_maxplayersperteam = gi.Cvar_GetOrCreate("sv_maxplayersperteam", "8", CVAR_SERVERINFO | CVAR_LATCH, "How many players (humans) may a team have");
	/* max. soldiers per team */
	sv_maxsoldiersperteam = gi.Cvar_GetOrCreate("sv_maxsoldiersperteam", "4", CVAR_ARCHIVE | CVAR_SERVERINFO, "How many soldiers may one team have");
	/* max soldiers per player */
	sv_maxsoldiersperplayer = gi.Cvar_GetOrCreate("sv_maxsoldiersperplayer", DOUBLEQUOTE(MAX_ACTIVETEAM), CVAR_ARCHIVE | CVAR_SERVERINFO, "How many soldiers one player is able to control in a given team");
	/* enable moralestates in multiplayer */
	sv_enablemorale = gi.Cvar_GetOrCreate("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "Enable morale behaviour for actors");
	sv_roundtimelimit = gi.Cvar_GetOrCreate("sv_roundtimelimit", "90", CVAR_ARCHIVE | CVAR_SERVERINFO, "Timelimit in seconds for multiplayer rounds");
	sv_roundtimelimit->modified = false;
	sv_maxentities = gi.Cvar_GetOrCreate("sv_maxentities", "1024", CVAR_LATCH, nullptr);

	sv_maxteams = gi.Cvar_GetOrCreate("sv_maxteams", "2", CVAR_SERVERINFO, "How many teams for current running map");
	sv_maxteams->modified = false;

	/* change anytime vars */
	password = gi.Cvar_GetOrCreate("password", "", CVAR_USERINFO, nullptr);
	sv_needpass = gi.Cvar_GetOrCreate("sv_needpass", "0", CVAR_SERVERINFO, nullptr);
	sv_filterban = gi.Cvar_GetOrCreate("sv_filterban", "1", 0, nullptr);
	sv_ai = gi.Cvar_GetOrCreate("sv_ai", "1", 0, "Activate or deativate the ai");
	sv_teamplay = gi.Cvar_GetOrCreate("sv_teamplay", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_SERVERINFO, "Is teamplay activated? see sv_maxclients, sv_maxplayersperteam, sv_maxsoldiersperteam and sv_maxsoldiersperplayer");
	/* how many connected clients */
	sv_maxclients = gi.Cvar_GetOrCreate("sv_maxclients", "1", CVAR_SERVERINFO, "If sv_maxclients is 1 we are in singleplayer - otherwise we are mutliplayer mode (see sv_teamplay)");
	sv_shot_origin = gi.Cvar_GetOrCreate("sv_shot_origin", "8", 0, "Assumed distance of muzzle from model");
	sv_send_edicts = gi.Cvar_GetOrCreate("sv_send_edicts", "0", CVAR_ARCHIVE | CVAR_CHEAT, "Send server side edicts for client display like triggers");
	sv_hurtaliens = gi.Cvar_GetOrCreate("sv_hurtaliens", "0", CVAR_SERVERINFO, "Spawn hurt aliens");

	ai_alienteam = gi.Cvar_GetOrCreate("ai_alienteam", "ortnok", 0, "Alien team");
	ai_civilianteam = gi.Cvar_GetOrCreate("ai_civilianteam", "europe", 0, "Civilian team");
	/* this cvar is set in singleplayer via campaign definition */
	ai_equipment = gi.Cvar_GetOrCreate("ai_equipment", "multiplayer_alien", 0, "Initial equipment definition for aliens");
	/* aliens in singleplayer (can differ each mission) */
	ai_singleplayeraliens = gi.Cvar_GetOrCreate("ai_singleplayeraliens", "30", 0, "How many aliens in this battle (singleplayer)");
	/* civilians for singleplayer */
	ai_numcivilians = gi.Cvar_GetOrCreate("ai_numcivilians", "10", 0, "How many civilians in this battle");
	/* aliens in multiplayer */
	ai_multiplayeraliens = gi.Cvar_GetOrCreate("ai_multiplayeraliens", "8", CVAR_ARCHIVE, "How many (ai controlled) actors in this battle (multiplayer)");

	mob_death = gi.Cvar_GetOrCreate("mob_death", "10", CVAR_LATCH|CVAR_NOSET, nullptr);
	mob_wound = gi.Cvar_GetOrCreate("mob_wound", "0.1", CVAR_LATCH|CVAR_NOSET, nullptr);
	mof_watching = gi.Cvar_GetOrCreate("mof_watching", "1.7", CVAR_LATCH|CVAR_NOSET, nullptr);
	mof_teamkill = gi.Cvar_GetOrCreate("mof_teamkill", "2.0", CVAR_LATCH|CVAR_NOSET, nullptr);
	mof_civilian = gi.Cvar_GetOrCreate("mof_civilian", "0.3", CVAR_LATCH|CVAR_NOSET, nullptr);
	mof_enemy = gi.Cvar_GetOrCreate("mof_ememy", "0.5", CVAR_LATCH|CVAR_NOSET, nullptr);
	mor_pain = gi.Cvar_GetOrCreate("mof_pain", "3.6", CVAR_LATCH|CVAR_NOSET, nullptr);
	/* everyone gets this times morale damage */
	mor_default = gi.Cvar_GetOrCreate("mor_default", "0.3", CVAR_LATCH|CVAR_NOSET, "Everyone gets this times morale damage");
	/* at this distance the following two get halved (exponential scale) */
	mor_distance = gi.Cvar_GetOrCreate("mor_distance", "120", CVAR_LATCH|CVAR_NOSET, "At this distance the following two get halved (exponential scale)");
	/* at this distance the following two get halved (exponential scale) */
	mor_victim = gi.Cvar_GetOrCreate("mor_victim", "0.7", CVAR_LATCH|CVAR_NOSET, "At this distance the following two get halved (exponential scale)");
	/* at this distance the following two get halved (exponential scale) */
	mor_attacker = gi.Cvar_GetOrCreate("mor_attacker", "0.3", CVAR_LATCH|CVAR_NOSET, "At this distance the following two get halved (exponential scale)");
	/* how much the morale depends on the size of the damaged team */
	mon_teamfactor = gi.Cvar_GetOrCreate("mon_teamfactor", "0.6", CVAR_LATCH|CVAR_NOSET, "How much the morale depends on the size of the damaged team");

	mor_regeneration = gi.Cvar_GetOrCreate("mor_regeneration", "15", CVAR_LATCH|CVAR_NOSET, nullptr);
	mor_shaken = gi.Cvar_GetOrCreate("mor_shaken", "50", CVAR_LATCH|CVAR_NOSET, nullptr);
	mor_panic = gi.Cvar_GetOrCreate("mor_panic", "30", CVAR_LATCH|CVAR_NOSET, nullptr);
	mor_brave = gi.Cvar_GetOrCreate("mor_brave", "85", CVAR_LATCH|CVAR_NOSET, nullptr);

	m_sanity = gi.Cvar_GetOrCreate("m_sanity", "1.0", CVAR_LATCH|CVAR_NOSET, nullptr);
	m_rage = gi.Cvar_GetOrCreate("m_rage", "0.6", CVAR_LATCH|CVAR_NOSET, nullptr);
	m_rage_stop = gi.Cvar_GetOrCreate("m_rage_stop", "2.0", CVAR_LATCH|CVAR_NOSET, nullptr);
	m_panic_stop = gi.Cvar_GetOrCreate("m_panic_stop", "1.0", CVAR_LATCH|CVAR_NOSET, nullptr);

	g_endlessaliens = gi.Cvar_GetOrCreate("g_endlessaliens", "0", CVAR_LATCH|CVAR_SERVERINFO, "Spawn endless aliens");
	g_ailua = gi.Cvar_GetOrCreate("g_ailua", "0", 0, "Activate or deactivate the LUA AI");
	g_aihumans = gi.Cvar_GetOrCreate("g_aihumans", "0", CVAR_DEVELOPER, "Activate or deactivate the ai for human actors");
	g_aidebug = gi.Cvar_GetOrCreate("g_aidebug", "0", CVAR_DEVELOPER|CVAR_CHEAT, "All AI actors are visible");
	g_drawtraces = gi.Cvar_GetOrCreate("g_drawtraces", "0", CVAR_DEVELOPER, "All traces will be rendered");
	g_nodamage = gi.Cvar_GetOrCreate("g_nodamage", "0", CVAR_DEVELOPER|CVAR_CHEAT, "No damage in developer mode");
	g_notu = gi.Cvar_GetOrCreate("g_notu", "0", CVAR_DEVELOPER|CVAR_CHEAT, "No TU costs while performing any action");
	g_actorspeed = gi.Cvar_GetOrCreate("g_actorspeed", "1.0", CVAR_ARCHIVE|CVAR_SERVERINFO, "Moving speed of the actor");
	g_lastseen = gi.Cvar_GetOrCreate("g_lastseen", "20", CVAR_ARCHIVE|CVAR_SERVERINFO, "Quit the match if no player was seen in this amount of rounds");
	g_nospawn = gi.Cvar_GetOrCreate("g_nospawn", "0", CVAR_DEVELOPER|CVAR_CHEAT, "Do not spawn a soldier");

	/* flood control */
	flood_msgs = gi.Cvar_GetOrCreate("flood_msgs", "4", 0, nullptr);
	flood_persecond = gi.Cvar_GetOrCreate("flood_persecond", "4", 0, nullptr);
	flood_waitdelay = gi.Cvar_GetOrCreate("flood_waitdelay", "10", 0, "Delay until someone is unlocked from talking again");

	g_difficulty = gi.Cvar_GetOrCreate("g_difficulty", "0", CVAR_NOSET, "Singleplayer difficulty level");

	game.sv_maxentities = sv_maxentities->integer;
	game.sv_maxplayersperteam = sv_maxplayersperteam->integer;

	/* initialize the entity storage */
	globals.edicts = G_EdictsConstruct();
	globals.max_edicts = game.sv_maxentities;
	globals.num_edicts = game.sv_maxplayersperteam;

	/* initialize all players for this game */
	/* game.sv_maxplayersperteam for human controlled players
	 * + game.sv_maxplayer for ai */
	game.players = static_cast<player_t*>(G_TagMalloc(game.sv_maxplayersperteam * 2 * sizeof(game.players[0]), TAG_GAME));
	globals.players = game.players;
	globals.maxplayersperteam = game.sv_maxplayersperteam;

	/* init csi and inventory */
	INVSH_InitCSI(gi.csi);
	game.invi.initInventory("game", gi.csi, &inventoryImport);

	if (logstats->integer)
		logstatsfile = fopen(va("%s/stats.log", gi.FS_Gamedir()), "a");
	else
		logstatsfile = nullptr;

	AI_Init();
	AIL_Init();
}

/**
 * @brief Free the tags TAG_LEVEL and TAG_GAME
 * @sa Mem_FreeTags
 */
static void G_Shutdown (void)
{
	gi.DPrintf("==== ShutdownGame ====\n");

	AIL_Shutdown();

	if (logstatsfile)
		fclose(logstatsfile);
	logstatsfile = nullptr;

	G_FreeTags(TAG_LEVEL);
	G_FreeTags(TAG_GAME);
	G_FreeAllInventory();

	Com_Printf("Used inventory slots in game on shutdown: %i\n", game.invi.GetUsedSlots());
}

/**
 * @brief If password has changed, update sv_needpass cvar as needed
 */
static void CheckNeedPass (void)
{
	if (password->modified) {
		password->modified = false;

		const int need = Q_strvalid(password->string) && Q_strcasecmp(password->string, "none") ? 1 : 0;
		gi.Cvar_Set("sv_needpass", "%i", need);
	}
}

static void G_SendBoundingBoxes (void)
{
	if (!sv_send_edicts->integer)
		return;

	Edict* ent = G_EdictsGetFirst();	/* skip the world */
	while ((ent = G_EdictsGetNextInUse(ent)))
		G_EventSendEdict(*ent);
}

/**
 * @sa SV_RunGameFrame
 * @sa G_MatchEndTrigger
 * @sa AI_Run
 * @return true if game reaches its end - false otherwise
 */
static bool G_RunFrame (void)
{
	level.framenum++;
	/* server is running at 10 fps */
	level.time = level.framenum * SERVER_FRAME_SECONDS;

	/* this doesn't belong here, but it works */
	if (!level.routed) {
		level.routed = true;
		G_CompleteRecalcRouting();
	}

	/* still waiting for other players */
	if (!G_MatchIsRunning()) {
		if (sv_maxteams->modified) {
			/* inform the client */
			gi.ConfigString(CS_MAXTEAMS, "%i", sv_maxteams->integer);
			sv_maxteams->modified = false;
		}
	}

	if (G_IsMultiPlayer()) {
		if (sv_roundtimelimit->modified) {
			/* some played around here - restart the count down */
			level.roundstartTime = level.time;
			/* don't allow smaller values here */
			if (sv_roundtimelimit->integer < 30 && sv_roundtimelimit->integer > 0) {
				gi.DPrintf("The minimum value for sv_roundtimelimit is 30\n");
				gi.Cvar_Set("sv_roundtimelimit", "30");
			}
			sv_roundtimelimit->modified = false;
		}
		G_CheckForceEndRound();
	}

	/* end this game? */
	if (G_MatchDoEnd())
		return true;

	CheckNeedPass();

	/* run ai */
	AI_Run();

	/* not all teams are spawned or game has already ended */
	if (G_MatchIsRunning())
		G_EdictsThink();

	G_SendBoundingBoxes();

	return false;
}

/**
 * @brief Returns a pointer to the structure with all entry points and global variables
 */
game_export_t* GetGameAPI (game_import_t* import)
{
	gi = *import;
	srand(gi.seed);

	globals.apiversion = GAME_API_VERSION;
	globals.Init = G_Init;
	globals.Shutdown = G_Shutdown;
	globals.SpawnEntities = G_SpawnEntities;

	globals.ClientConnect = G_ClientConnect;
	globals.ClientUserinfoChanged = G_ClientUserinfoChanged;
	globals.ClientDisconnect = G_ClientDisconnect;
	globals.ClientBegin = G_ClientBegin;
	globals.ClientStartMatch = G_ClientStartMatch;
	globals.ClientCommand = G_ClientCommand;
	globals.ClientAction = G_ClientAction;
	globals.ClientEndRound = G_ClientEndRound;
	globals.ClientTeamInfo = G_ClientTeamInfo;
	globals.ClientInitActorStates = G_ClientInitActorStates;
	globals.ClientGetTeamNum = G_ClientGetTeamNum;
	globals.ClientIsReady = G_ClientIsReady;
	globals.ClientGetName = G_GetPlayerName;
	globals.ClientGetActiveTeam = G_GetActiveTeam;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = G_ServerCommand;

	globals.edict_size = sizeof(Edict);
	globals.player_size = sizeof(player_t);

	return &globals;
}

#ifndef HARD_LINKED_GAME
/* this is only here so the functions in the shared code can link */
void Sys_Error (const char* error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	gi.Error("%s", text);
}

void Com_Printf (const char* msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	gi.DPrintf("%s", text);
}

void Com_DPrintf (int level, const char* msg, ...)
{
	va_list argptr;
	char text[1024];

	/* don't confuse non-developers with techie stuff... */
	if (!developer || developer->integer == 0)
		return;

	if (!(developer->integer & level))
		return;

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	gi.DPrintf("%s", text);
}
#endif
