//===========================================================================//
//
// Author: Nanoman2525 & NULLderef
// Maintainer: Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//
#include "p2mm.hpp"

#include "discordrpc.hpp"

#include "minhook/include/MinHook.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar p2mm_discord_rpc;
extern ConVar p2mm_discord_webhooks;

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
IVEngineServer* engineServer = NULL; // Access engine server functions (messaging clients, loading content, making entities, running commands, etc).
IVEngineClient* engineClient = NULL; // Access engine client functions.
CGlobalVars* g_pGlobals = NULL; // Access global variables shared between the engine and games dlls.
IPlayerInfoManager* g_pPlayerInfoManager = NULL; // Access interface functions for players.
IScriptVM* g_pScriptVM = NULL; // Access VScript interface.
IServerTools* g_pServerTools = NULL; // Access to interface from engine to tools for manipulating entities.
IGameEventManager2* g_pGameEventManager_ = NULL; // Access game events interface.
IServerPluginHelpers* g_pPluginHelpers = NULL; // Access interface for plugin helper functions.
IFileSystem* g_pFileSystem = NULL; // Access interface for Valve's file system interface.
#ifndef GAME_DLL
#define g_pGameEventManager g_pGameEventManager_
#endif

//---------------------------------------------------------------------------------
// Class declarations/creations
//---------------------------------------------------------------------------------
CDiscordIntegration* g_pDiscordIntegration = new CDiscordIntegration;

//---------------------------------------------------------------------------------
// The plugin is a static singleton that is exported as an interface
//---------------------------------------------------------------------------------
CP2MMServerPlugin g_P2MMServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CP2MMServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_P2MMServerPlugin);

// List of game events the plugin interfaces used to load each one.
static const char* gameEventList[] =
{
	"portal_player_ping",
	"portal_player_portaled",
	"turret_hit_turret",
	"security_camera_detached",
	"player_landed",
	"player_spawn_blue",
	"player_spawn_orange",
	"player_death",
	"player_spawn",
	"player_connect",
	"player_say",
	"player_activate",
};

// List of console commands that clients can't execute but the host can.
static const char* forbiddenConCommands[] =
{
	"mp_earn_taunt",
	"mp_mark_all_maps_complete",
	"mp_mark_all_maps_incomplete",
	"mp_mark_course_complete",
	"report_entities",
	"script", // Valve patched this, here just in case
	"script_debug", // Valve patched this, here just in case
	"script_dump_all", // Valve patched this, here just in case
	"script_execute", // Valve patched this, here just in case
	"script_help", // Valve patched this, here just in case
	"script_reload_code", // Valve patched this, here just in case
	"script_reload_entity_code", // Valve patched this, here just in case
	"script_reload_think", // Valve patched this, here just in case
	"ent_fire",
	"fire_rocket_projectile",
	"fire_energy_ball",
	"ent_remove",
	"ent_remove_all"
};

// List of client commands that need to be blocked from client execution, but can be executed by the host.
static const char* forbiddenClientCommands[] =
{
	"taunt_auto", // Apparently mp_earn_taunt calls this also to ClientCommand
	"restart_level",
	"pre_go_to_hub",
	"pre_go_to_calibration",
	"go_to_calibration",
	"go_to_hub",
	"restart_level",
	"mp_restart_level",
	"transition_map",
	"select_map",
	"mp_select_level",
	"erase_mp_progress",
	"bugpause",
	"bugunpause"
};

//---------------------------------------------------------------------------------
// Core P2:MM ConVars | These shouldn't be modified manually. Hidden to prevent accidentally breaking something.
//---------------------------------------------------------------------------------
ConVar p2mm_loop("p2mm_loop", "0", FCVAR_HIDDEN, "Flag if P2MMLoop should be looping.");
ConVar p2mm_lastmap("p2mm_lastmap", "", FCVAR_HIDDEN, "Last map recorded for the Last Map system.");
ConVar p2mm_splitscreen("p2mm_splitscreen", "0", FCVAR_HIDDEN, "Flag for the main menu buttons and launcher to start in splitscreen or not.");

//---------------------------------------------------------------------------------
// UTIL P2:MM ConVars | ConVars the host can change.
//---------------------------------------------------------------------------------
ConVar p2mm_forbidclientcommands("p2mm_forbidclientcommands", "1", FCVAR_NONE, "Stop client commands clients shouldn't be executing.");
ConVar p2mm_deathicons("p2mm_deathicons", "1", FCVAR_NONE, "Whether or not when players die the death icon should appear.");
ConVar p2mm_ds_enable_paint("p2mm_ds_enable_paint", "0", FCVAR_NONE, "Re-enables gel functionality in dedicated servers on the next map load.");
ConVar p2mm_instant_respawn("p2mm_instant_respawn", "0", FCVAR_NONE, "Whether respawning should be instant or not.");

//---------------------------------------------------------------------------------
// Debug P2:MM ConVars | Self-explanatory.
//---------------------------------------------------------------------------------
ConVar p2mm_developer("p2mm_developer", "0", FCVAR_NONE, "Enable for P2:MM developer messages.");

void UpdateDisplayGEsConVar(IConVar* var, const char* pOldValue, float flOldValue)
{
	ConVar* pGEConVar = g_pCVar->FindVar("display_game_events");
	if (pGEConVar)
		pGEConVar->SetValue(((ConVar*)var)->GetBool());
}
ConVar p2mm_spewgameeventinfo("p2mm_spewgameeventinfo", "0", FCVAR_NONE, "Log information from called game events in the console, p2mm_developer must also be on. Can cause lots of console spam.", UpdateDisplayGEsConVar);

//---------------------------------------------------------------------------------
// P2:MM ConCommands
//---------------------------------------------------------------------------------
std::vector<std::string> mapList; // List of maps for the p2mm_map command auto complete.
std::vector<std::string> workshopMapList; // List of all workshop map for the p2mm_map auto complete.

// Update the map list avaliable to p2mm_map by scanning for all map files in SearchPath.
void updateMapsList() {
	mapList.clear();
	CUtlVector<CUtlString> outList;
	AddFilesToList(outList, "maps", "GAME", "bsp");

	FOR_EACH_VEC(outList, i)
	{
		// Get each map and get their relative path to each SearchPath and make slashes forward slashes.
		// Then turn relativePath into a std::string to easily manipulate.
		const char* curmap = outList[i];
		char relativePath[MAX_PATH] = { 0 };
		g_pFileSystem->FullPathToRelativePathEx(curmap, "GAME", relativePath, sizeof(relativePath));
		V_FixSlashes(relativePath, '/');
		V_StripExtension(relativePath, relativePath, sizeof(relativePath));
		std::string fixedRelativePath(relativePath);

		// Remove "maps/" out of the string.
		fixedRelativePath.erase(0, strlen("maps/"));

		// Remove the whole "workshop/(workshop id)" part if there isn't multiple workshop maps of the same file name.
		size_t lastSlashPos = fixedRelativePath.find_last_of("/");
		if (lastSlashPos != std::string::npos && fixedRelativePath.rfind("workshop/") != std::string::npos)
		{
			fixedRelativePath.erase(0, strlen("workshop/"));
			workshopMapList.push_back(fixedRelativePath);
		}

		// Push the map string on to the list to display avaliable options for the command.
		mapList.push_back(std::move(fixedRelativePath));
	}
}

// Autocomplete for p2mm_map.
static int p2mm_map_CompletionFunc(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	// If the map list is empty, generate it.
	if (mapList.empty()) {
		updateMapsList();
	}

	// Assemble together the current state of the inputted command.
	const char* concommand = "p2mm_map ";
	const char* match = (V_strstr(partial, concommand) == partial) ? partial + V_strlen(concommand) : partial;

	// Go through the map list searching for matches with the assembled inputted command.
	int numMatchedMaps = 0;
	for (const std::string map : mapList)
	{
		if (numMatchedMaps >= COMMAND_COMPLETION_MAXITEMS) break;

		if (V_strstr(map.c_str(), match))
		{
			V_snprintf(commands[numMatchedMaps++], COMMAND_COMPLETION_ITEM_LENGTH, "%s%s", concommand, map.c_str());
		}
	}

	return numMatchedMaps;
}

CON_COMMAND_F_COMPLETION(p2mm_map, "Starts up a P2:MM session with a requested map.", FCVAR_NONE, p2mm_map_CompletionFunc)
{
	// If the map list is empty, generate it.
	if (mapList.empty()) {
		updateMapsList();
	}

	// Make sure the CON_COMMAND was executed correctly.
	if (args.ArgC() < 2 || FStrEq(args.Arg(1), ""))
	{
		P2MMLog(1, false, "p2mm_map called incorrectly! Usage: \"p2mm_map (map to start)\"");
		updateMapsList();
		return;
	}

	// A check done by the menu to request to use the last recorded map in the p2mm_lastmap ConVar.
	char requestedMap[256] = { 0 };
	V_strcpy(requestedMap, args.Arg(1));
	P2MMLog(0, true, "Requested Map: %s", requestedMap);
	P2MMLog(0, true, "p2mm_lastmap: %s", p2mm_lastmap.GetString());
	if (FStrEq(requestedMap, "P2MM_LASTMAP"))
	{
		if (!engineServer->IsMapValid(p2mm_lastmap.GetString()))
		{
			// Running disconnect to make the error screen appear causes the music to stop, don't let that to happen, so here it is started it again.

			// Get the current act so we can start the right main menu music
			int iAct = ConVarRef("ui_lastact_played").GetInt();
			if (iAct > 5) iAct = 5;
			else if (iAct < 1) iAct = 1;

			// Put the command to start the music and the act number together.
			char completePVCmd[sizeof("playvol \"#music/mainmenu/portal2_background0%d\" 0.35") + sizeof(iAct)] = { 0 };
			V_snprintf(completePVCmd, sizeof(completePVCmd), "playvol \"#music/mainmenu/portal2_background0%i\" 0.35", iAct);

			P2MMLog(1, false, "p2mm_map was called with P2MM_LASTMAP, but p2mm_lastmap is empty or invalid!");
			engineClient->ExecuteClientCmd("disconnect \"There is no last map recorded or the map doesn't exist! Please start a play session with the other options first.\"");
			engineClient->ExecuteClientCmd(completePVCmd);
			updateMapsList();
			return;
		}
		V_strcpy(requestedMap, p2mm_lastmap.GetString());
		P2MMLog(0, true, "P2MM_LASTMAP called! Running Last Map: \"%s\"", requestedMap);
	}
	p2mm_lastmap.SetValue(""); // Set last map ConVar to blank so it doesn't trigger level changes where we don't want it to trigger.

	// Check if the requested map is a workshop map.
	std::string tempMapStr = requestedMap;
	for (const std::string map : workshopMapList)
	{
		if (tempMapStr == map)
		{
			tempMapStr = std::string("workshop/" + tempMapStr);
			V_strcpy(requestedMap, tempMapStr.c_str());
		}
	}

	// Check if the supplied map is a valid map.
	if (!engineServer->IsMapValid(requestedMap))
	{
		P2MMLog(1, false, "p2mm_map was given a non-valid map or one that doesn't exist! \"%s\"", requestedMap);
		updateMapsList();
		return;
	}

	// Check if the user requested it to start in splitscreen or not.
	std::string mapString = p2mm_splitscreen.GetBool() ? "ss_map " : "map ";
	P2MMLog(0, true, "Map String: %s", mapString.c_str());

	// Set first run flag on and set the last map ConVar value so the system.
	// can change from mp_coop_community_hub to the requested map.
	// Also set m_bSeenFirstRunPrompt back to false so the prompt can be triggered again.
	g_P2MMServerPlugin.m_bFirstMapRan = true;
	g_P2MMServerPlugin.m_bSeenFirstRunPrompt = false;
	if (!FSubStr(requestedMap, "mp_coop"))
	{
		P2MMLog(0, true, "\"mp_coop\" not found, singleplayer map being run. Full ExecuteClientCmd: \"%s\"", std::string(mapString + "mp_coop_community_hub").c_str());
		P2MMLog(0, true, "requestedMap: \"%s\"", requestedMap);
		p2mm_lastmap.SetValue(requestedMap);
		engineClient->ExecuteClientCmd(std::string(mapString + "mp_coop_community_hub").c_str());

		std::string initmapstr = std::string("Server has started with map: `" + std::string(requestedMap) + "`");
		g_pDiscordIntegration->SendWebHookEmbed(std::string("Server"), initmapstr, EMBEDCOLOR_SERVER, false);
	}
	else
	{
		P2MMLog(0, true, "\"mp_coop\" found, multiplayer map being run. Full ExecuteClientCmd: \"%s\"", std::string(mapString + requestedMap).c_str());
		P2MMLog(0, true, "requestedMap: \"%s\"", requestedMap);
		engineClient->ExecuteClientCmd(std::string(mapString + requestedMap).c_str());

		std::string initmapstr = std::string("Server has started with map: `" + std::string(requestedMap) + "`");
		g_pDiscordIntegration->SendWebHookEmbed(std::string("Server"), initmapstr, EMBEDCOLOR_SERVER, false);
	}
}

CON_COMMAND(p2mm_updatemaplist, "Manually updates the list of available maps that can be loaded with p2mm_map.")
{
	updateMapsList();
}

CON_COMMAND(p2mm_maplist, "Lists available maps that can be loaded with p2mm_map.")
{
	P2MMLog(0, false, "AVALIABLE MAPS:");
	P2MMLog(0, false, "----------------------------------------");
	for (const std::string map : mapList)
	{
		P2MMLog(0, false, map.c_str());
	}
	P2MMLog(0, false, "----------------------------------------");
}


//---------------------------------------------------------------------------------
// Utility P2:MM ConCommands
//---------------------------------------------------------------------------------
CON_COMMAND(p2mm_respawnall, "Respawns all players.")
{
	FOR_ALL_PLAYERS(i)
	{
		player_info_t playerInfo;
		if (engineServer->GetPlayerInfo(i, &playerInfo))
			CPortal_Player__RespawnPlayer(i);
	}
}

bool m_ConVarConCommandsShown = false; // Bool to track if the hidden ConVars and ConCommands are showing.
std::vector<ConCommandBase*> toggledCVCCs; // List of toggled ConVars and ConCommands with the FCVAR_DEVELOPMENTONLY and FCVAR_HIDDEN ConVar flags removed.
CON_COMMAND_F(p2mm_toggle_dev_cc_cvars, "Toggle showing any ConVars and ConCommands that have the FCVAR_DEVELOPMENTONLY and FCVAR_HIDDEN ConVar flags.", FCVAR_HIDDEN)
{
	int iToggleCount = 0; // To tell the user how many ConVars and ConCommands where toggle to show or hide.

	if (m_ConVarConCommandsShown)
	{
		// Hide the ConVars and ConCommands
		for (ConCommandBase* pCommandVarName : toggledCVCCs)
		{
			pCommandVarName->AddFlags(FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN);
			iToggleCount++;
		}
		toggledCVCCs.clear();
		m_ConVarConCommandsShown = false;
	}
	else
	{
		// Unhide the ConVars and ConCommands
		FOR_ALL_CONSOLE_COMMANDS(pCommandVarName)
		{
			if (pCommandVarName->IsFlagSet(FCVAR_DEVELOPMENTONLY) || pCommandVarName->IsFlagSet(FCVAR_HIDDEN))
			{
				pCommandVarName->RemoveFlags(FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN);
				iToggleCount++;
				toggledCVCCs.push_back(pCommandVarName);
			}
		}
		m_ConVarConCommandsShown = true;
	}

	P2MMLog(0, false, "%s %i ConVars/ConCommands!", m_ConVarConCommandsShown ? "Unhid" : "Hid", iToggleCount);
}

//---------------------------------------------------------------------------------
// Debug P2:MM ConCommands
//---------------------------------------------------------------------------------
CON_COMMAND_F(p2mm_helloworld, "Hello World!", FCVAR_HIDDEN)
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(1);
	UTIL_ClientPrint(pPlayer, HUD_PRINTCENTER, "HELLO WORLD! :D\n%s\n%s", "test1", "test2");
}

CON_COMMAND_F(p2mm_helloworld2, "Hello World 2: Electric Boogaloo!", FCVAR_HIDDEN)
{
	hudtextparms_t helloWorldParams;
	color32 RGB1 = { 0, 255, 100, 255 };
	color32 RGB2 = { 0, 50, 255, 255 };
	helloWorldParams.x = -1.f;
	helloWorldParams.y = -1.f;
	helloWorldParams.effect = 2;
	helloWorldParams.fxTime = 0.2f;
	helloWorldParams.r1 = RGB1.r;
	helloWorldParams.g1 = RGB1.g;
	helloWorldParams.b1 = RGB1.b;
	helloWorldParams.a1 = RGB1.a;
	helloWorldParams.r2 = RGB2.r;
	helloWorldParams.g2 = RGB2.g;
	helloWorldParams.b2 = RGB2.b;
	helloWorldParams.a2 = RGB2.a;
	helloWorldParams.fadeinTime = 0.5f;
	helloWorldParams.fadeoutTime = 1.f;
	helloWorldParams.holdTime = 1.f;

	helloWorldParams.channel = 3;
	if (!FStrEq(args.Arg(1), ""))
		helloWorldParams.channel = V_atoi(args.Arg(1));

	const char* msg = "Hello World 2: Electric Boogaloo!";
	if (!FStrEq(args.Arg(2), ""))
		msg = args.Arg(2);

	UTIL_HudMessage(UTIL_PlayerByIndex(1), helloWorldParams, msg);
}

//---------------------------------------------------------------------------------
// P2:MM Gelocity ConVars and ConCommands
//---------------------------------------------------------------------------------
ConVar p2mm_gelocity_laps_default("p2mm_gelocity_laps_default", "3", FCVAR_NONE, "Set the default amount of laps for a Gelocity race.", true, 1, true, 300);
ConVar p2mm_gelocity_music_default("p2mm_gelocity_music_default", "0", FCVAR_NONE, "Set the default music track for a Gelocity race.", true, 0, true, 5);

void GelocityTournament(IConVar* var, const char* pOldValue, float flOldValue)
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		P2MMLog(0, false, "Gelocity tournament mode ConVar was changed from %i to %i.", (int)flOldValue, ((ConVar*)var)->GetBool());
		P2MMLog(1, false, "Mode will take effect when Gelocity map is loaded.");
		return;
	}

	// Check if the gelocity race is already going. Make sure to not mess with the race's laps.
	ScriptVariant_t raceStartedScript;
	g_pScriptVM->GetValue("b_RaceStarted", &raceStartedScript);
	if (raceStartedScript.m_bool)
	{
		P2MMLog(1, false, "Race is currently in progress!");
		return;
	}

	P2MMLog(0, false, "Gelocity tournament mode ConVar was changed from %i to %i!", (int)flOldValue, ((ConVar*)var)->GetBool());
	P2MMLog(1, false, "Restarting map based on tournament mode change!");

	engineClient->ExecuteClientCmd(std::string("changelevel " + std::string(CURMAPFILENAME)).c_str());
}
ConVar p2mm_gelocity_tournamentmode("p2mm_gelocity_tournamentmode", "0", FCVAR_NONE, "Turn on or off tournament mode.", true, 0, true, 1, GelocityTournament);

void GelocityButtons(IConVar* var, const char* pOldValue, float flOldValue)
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		if (!((ConVar*)var)->GetBool())
			P2MMLog(0, false, "Unlocked buttons...");
		else
			P2MMLog(0, false, "Locked buttons...");
		P2MMLog(1, false, "Mode will take effect when Gelocity map is loaded.");
		return;
	}

	// Lock or unlock the buttons.
	if (!((ConVar*)var)->GetBool())
	{
		g_pScriptVM->Run(
			"EntFire(\"rounds_button_1\", \"Unlock\");"
			"EntFire(\"rounds_button_2\", \"Unlock\");"
			"EntFire(\"music_button_1\", \"Unlock\");"
			"EntFire(\"music_button_2\", \"Unlock\");", false
		);
		P2MMLog(0, false, "Unlocked buttons...");
	}
	else
	{
		g_pScriptVM->Run(
			"EntFire(\"rounds_button_1\", \"Lock\");"
			"EntFire(\"rounds_button_2\", \"Lock\");"
			"EntFire(\"music_button_1\", \"Lock\");"
			"EntFire(\"music_button_2\", \"Lock\");", false
		);
		P2MMLog(0, false, "Locked buttons...");
	}
}
ConVar p2mm_gelocity_lockbuttons("p2mm_gelocity_lockbuttons", "0", FCVAR_NONE, "Toggle the state of the music and lap buttons.", true, 0, true, 1, GelocityButtons);

CON_COMMAND(p2mm_gelocity_laps, "Set lap count for the Gelocity Race. Specify 0 or no argument to see current lap count.")
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		P2MMLog(1, false, "Not currently in a Gelocity map!");
		return;
	}

	// Check if the gelocity race is already going. Make sure to not mess with the race's laps.
	ScriptVariant_t raceStartedScript;
	g_pScriptVM->GetValue("bRaceStarted", &raceStartedScript);
	if (raceStartedScript.m_bool)
	{
		P2MMLog(1, false, "Race is currently in progress!");
		return;
	}

	// Check if 0 or no arguments are specified so that the ConCommand
	// returns how many laps are currently set.
	// But if it's a value out of range, return error.
	if (V_atoi(args.Arg(1)) == 0 || args.ArgC() == 1)
	{
		ScriptVariant_t raceLaps;
		g_pScriptVM->GetValue("iGameLaps", &raceLaps);
		P2MMLog(0, false, "Current race laps: %i", raceLaps.m_int);
		return;
	}
	else if (V_atoi(args.Arg(1)) < 1 || V_atoi(args.Arg(1)) > 300)
	{
		P2MMLog(1, false, "Value out of bounds! Lap counter goes from 1-300!");
		return;
	}

	g_pScriptVM->Run(std::string("iGameLaps <- " + std::string(args.Arg(1))).c_str(), false);
	hudtextparms_s lapMessage;
	lapMessage.x = -1;
	lapMessage.y = 0.2f;
	lapMessage.effect = 0;
	lapMessage.r1 = 255;
	lapMessage.g1 = 255;
	lapMessage.b1 = 255;
	lapMessage.a1 = 255;
	lapMessage.r2 = 0;
	lapMessage.g2 = 0;
	lapMessage.b2 = 0;
	lapMessage.a1 = 0;
	lapMessage.fadeinTime = 0.5f;
	lapMessage.fadeoutTime = 0.5f;
	lapMessage.holdTime = 1.f;
	lapMessage.fxTime = 0.f;
	lapMessage.channel = 3;

	UTIL_HudMessage(NULL, lapMessage, std::string("Race Laps: " + std::string(args.Arg(1))).c_str());
}

CON_COMMAND(p2mm_gelocity_music, "Set the music track for the Gelocity Race. 0-5 0 = No Music.")
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		P2MMLog(1, false, "Not currently in a Gelocity map!");
		return;
	}

	// Check if the final lap has been triggered. LET THE INTENSE FINAL LAP MUSIC PLAY!
	ScriptVariant_t finalLapScript;
	g_pScriptVM->GetValue("bFinalLap", &finalLapScript);
	if (finalLapScript.m_bool)
	{
		P2MMLog(1, false, "ITS THE FINAL LAP! LET THE INTENSE FINAL LAP MUSIC PLAY!");
		return;
	}

	// Check if value is out of bounds.
	if (args.ArgC() == 1)
	{
		ScriptVariant_t iMusicTrack;
		g_pScriptVM->GetValue("iMusicTrack", &iMusicTrack);
		P2MMLog(0, false, "Current music track: %i", iMusicTrack.m_int);
		return;
	}
	else if (V_atoi(args.Arg(1)) < 0 || V_atoi(args.Arg(1)) > 5)
	{
		P2MMLog(1, false, "Value out of bounds! Music tracks goes from 0-5!");
		return;
	}

	g_pScriptVM->Run(std::string("iMusicTrack <- " + std::to_string(V_atoi(args.Arg(1))) + "; EntFire(\"counter_music\", \"SetValue\", iMusicTrack.tostring());").c_str(), false);
	hudtextparms_s musicMessage;
	musicMessage.x = -1;
	musicMessage.y = 0.2f;
	musicMessage.effect = 0;
	musicMessage.r1 = 255;
	musicMessage.g1 = 255;
	musicMessage.b1 = 255;
	musicMessage.a1 = 255;
	musicMessage.r2 = 0;
	musicMessage.g2 = 0;
	musicMessage.b2 = 0;
	musicMessage.a1 = 0;
	musicMessage.fadeinTime = 0.5f;
	musicMessage.fadeoutTime = 0.5f;
	musicMessage.holdTime = 1.f;
	musicMessage.fxTime = 0.f;
	musicMessage.channel = 3;

	if (V_atoi(args.Arg(1)) == 0)
	{
		UTIL_HudMessage(NULL, musicMessage, std::string("No Music").c_str());
		P2MMLog(0, false, "Music turned off!", V_atoi(args.Arg(1)));
	}
	else
	{
		UTIL_HudMessage(NULL, musicMessage, std::string("Music Track: " + std::string(args.Arg(1))).c_str());
		P2MMLog(0, false, "Set music track to %i!", V_atoi(args.Arg(1)));
	}
}

CON_COMMAND(p2mm_gelocity_start, "Starts the Gelocity race.")
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		P2MMLog(1, false, "Not currently in a Gelocity map!");
		return;
	}

	// Check if the gelocity race is already going. Make sure to not mess with the race's laps.
	ScriptVariant_t raceStartedScript;
	g_pScriptVM->GetValue("b_RaceStarted", &raceStartedScript);
	if (raceStartedScript.m_bool)
	{
		P2MMLog(1, false, "Race is currently in progress!");
		return;
	}

	g_pScriptVM->Run("StartGelocityRace();", false);
}

//---------------------------------------------------------------------------------
// Purpose: constructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::CP2MMServerPlugin()
{
	// Store game vars
	this->m_bSeenFirstRunPrompt = false;	// Flag is set true after CallFirstRunPrompt() is called in VScript.
	this->m_bFirstMapRan = true;			// Checks if the game ran for the first time.
	this->sv = nullptr;						// Pointer to the server.

	// Store plugin status
	this->m_bPluginLoaded = false;
	this->m_bPluginUnloading = false;		// For Discord RPC.
	this->m_bNoUnload = false;				// If we fail to load, we don't want to run anything on Unload().

	// Current Portal 2 branch based game being run.
	// Helps when checking for specific game related things instead of getting the game directory everytime.
	// Portal 2: 0
	// Portal Stories: Mel: 1
	this->m_iCurGameIndex = 0;

	m_nDebugID = EVENT_DEBUG_ID_INIT;
	this->m_iClientCommandIndex = 0;
}

//---------------------------------------------------------------------------------
// Purpose: destructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::~CP2MMServerPlugin()
{
	m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
}

//---------------------------------------------------------------------------------
// Purpose: Description of plugin outputted when the "plugin_print" console command is executed.
//---------------------------------------------------------------------------------
const char* CP2MMServerPlugin::GetPluginDescription(void)
{
	return "Portal 2: Multiplayer Mod Server Plugin | Plugin Version: " P2MM_PLUGIN_VERSION " | For P2:MM Version: " P2MM_VERSION;
}

// NoSteamLogon stop hook.
class CSteam3Server;
class CBaseClient;
void(__fastcall* CSteam3Server__OnGSClientDenyHelper_orig)(CSteam3Server* thisptr, void* edx, CBaseClient* cl, void* eDenyReason, const char* pchOptionalText);
void __fastcall CSteam3Server__OnGSClientDenyHelper_hook(CSteam3Server* thisptr, void* edx, CBaseClient* cl, void* eDenyReason, const char* pchOptionalText)
{
	// If we the game attempts to disconnect with "No Steam Logon", here we just tell it no.
	if ((int)eDenyReason == 0xC)
		return;

	CSteam3Server__OnGSClientDenyHelper_orig(thisptr, edx, cl, eDenyReason, pchOptionalText);
}

// Bottom three hooks are for being able to change the starting models to something different.
// First two are for changing what model is returned when precaching however...
// Last one is for actually specifying the right model as the MSVC compiler inlined the returns
// of the original functions of these first two hooks. Thanks MSVC for making this hard. :D
const char* (__cdecl* GetBallBotModel_orig)(bool bLowRes);
const char* __cdecl GetBallBotModel_hook(bool bLowRes)
{
	if (g_P2MMServerPlugin.m_iCurGameIndex == 1)
		return "models/portal_stories/player/mel.mdl";

	return GetBallBotModel_orig(bLowRes);
}

const char* (__cdecl* GetEggBotModel_orig)(bool bLowRes);
const char* __cdecl GetEggBotModel_hook(bool bLowRes)
{
	if (g_P2MMServerPlugin.m_iCurGameIndex == 1)
		return "models/player/chell/player.mdl";

	return GetEggBotModel_orig(bLowRes);
}

const char* (__fastcall* CPortal_Player__GetPlayerModelName_orig)(CPortal_Player* thisptr);
const char* __fastcall CPortal_Player__GetPlayerModelName_hook(CPortal_Player* thisptr)
{
	if (g_P2MMServerPlugin.m_iCurGameIndex == 1)
	{
		if (CBaseEntity__GetTeamNumber((CBasePlayer*)thisptr) == TEAM_BLUE)
			return "models/portal_stories/player/mel.mdl";
		else
			return "models/player/chell/player.mdl";
	}
	return CPortal_Player__GetPlayerModelName_orig(thisptr);
}

// For hooking onto the function that is called before a player respawns to skip the delay
// that is usual there and instead force a instant respawn of the player.
void(__fastcall* CPortal_Player__PlayerDeathThink_orig)(CPortal_Player* thisptr);
void __fastcall CPortal_Player__PlayerDeathThink_hook(CPortal_Player* thisptr)
{
	if (p2mm_instant_respawn.GetBool())
	{
		CPortal_Player__RespawnPlayer(ENTINDEX((CBaseEntity*)thisptr));
		return;
	}
	CPortal_Player__PlayerDeathThink_orig(thisptr);
}

void(__cdecl* respawn_orig)(CBaseEntity* pEdict, bool fCopyCorpse);
void __cdecl respawn_hook(CBaseEntity* pEdict, bool fCopyCorpse)
{
	respawn_orig(pEdict, fCopyCorpse);

	if (g_pScriptVM)
	{
		// Handling OnRespawn VScript event
		HSCRIPT or_func = g_pScriptVM->LookupFunction("OnRespawn");
		if (or_func)
			g_pScriptVM->Call<HSCRIPT>(or_func, NULL, false, NULL, INDEXHANDLE(ENTINDEX(pEdict)));

		// Handle VScript game event function
		HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerRespawn");
		if (ge_func)
			g_pScriptVM->Call<HSCRIPT>(ge_func, NULL, false, NULL, INDEXHANDLE(ENTINDEX(pEdict)));
	}
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is loaded, initialization process.
//			Loads the interfaces we need from the engine and applies our patches.
//---------------------------------------------------------------------------------
bool CP2MMServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	if (m_bPluginLoaded)
	{
		P2MMLog(1, false, "PLugin already loaded!");
		m_bNoUnload = true;
		return false;
	}

	P2MMLog(0, false, "Loading plugin...");

	// Determine which Portal 2 branch game we are running.
	P2MMLog(0, true, "Determining which Portal 2 branch game is being run...");
	this->m_iCurGameIndex = 0; // Portal 2
	if ((FStrEq(GetGameMainDir(), "portal_stories")))
		this->m_iCurGameIndex = 1; // Portal Stories: Mel

	if (p2mm_developer.GetBool())
	{
		switch (this->m_iCurGameIndex)
		{
		case 0:
			P2MMLog(0, true, "Currently running Portal 2.");
			break;
		case 1:
			P2MMLog(0, true, "Currently running Portal Stories: Mel.");
			break;
		default:
			P2MMLog(0, true, "Currently running Portal 2.");
			break;
		}
	}

	P2MMLog(0, true, "Connecting tier libraries...");
	ConnectTier1Libraries(&interfaceFactory, 1);
	ConnectTier2Libraries(&interfaceFactory, 1);

	// Make sure that all the interfaces needed are loaded and useable
	P2MMLog(0, true, "Loading interfaces...");
	engineServer = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, 0);
	if (!engineServer)
	{
		P2MMLog(1, false, "Unable to load engineServer!");
		this->m_bNoUnload = true;
		return false;
	}

	engineClient = (IVEngineClient*)interfaceFactory(VENGINE_CLIENT_INTERFACE_VERSION, 0);
	if (!engineClient)
	{
		P2MMLog(1, false, "Unable to load engineClient!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pPlayerInfoManager = (IPlayerInfoManager*)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, 0);
	if (!g_pPlayerInfoManager)
	{
		P2MMLog(1, false, "Unable to load g_pPlayerInfoManager!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pScriptVM = (IScriptVM*)interfaceFactory(VSCRIPT_INTERFACE_VERSION, 0);
	if (!g_pScriptVM)
	{
		P2MMLog(1, false, "Unable to load g_pScriptVM!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pServerTools = (IServerTools*)gameServerFactory(VSERVERTOOLS_INTERFACE_VERSION, 0);
	if (!g_pServerTools)
	{
		P2MMLog(1, false, "Unable to load g_pServerTools!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pGameEventManager = (IGameEventManager2*)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2, 0);
	if (!g_pGameEventManager)
	{
		P2MMLog(1, false, "Unable to load g_pGameEventManager!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pPluginHelpers = (IServerPluginHelpers*)interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, 0);
	if (!g_pPluginHelpers)
	{
		P2MMLog(1, false, "Unable to load g_pPluginHelpers!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pFileSystem = (IFileSystem*)interfaceFactory(FILESYSTEM_INTERFACE_VERSION, 0);
	if (!g_pFileSystem)
	{
		P2MMLog(1, false, "Unable to load g_pFileSystem!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pGlobals = g_pPlayerInfoManager->GetGlobalVars();
	MathLib_Init(2.2f, 2.2f, 0.0f, 2.0f);
	ConVar_Register(0);

	// Discord RPC
	P2MMLog(0, true, "Checking if Discord RPC should be started...");
	if (p2mm_discord_rpc.GetBool() && !g_pDiscordIntegration->RPCRunning)
		P2MMLog(0, true, "Discord RPC enabled! Starting!");
		g_pDiscordIntegration->StartDiscordRPC();

	// Add listener for all used game events
	P2MMLog(0, true, "Adding listeners for game events...");
	for (const char* gameevent : gameEventList)
	{
		g_pGameEventManager->AddListener(this, gameevent, true);
		P2MMLog(0, true, "Listener for game event \"%s\" has been added!", gameevent);
	}

	// Block ConCommands that clients shouldn't execute
	P2MMLog(0, true, "Blocking console commands...");
	for (const char* concommand : forbiddenConCommands)
	{
		ConCommandBase* commandbase = g_pCVar->FindCommandBase(concommand);
		if (commandbase)
			commandbase->RemoveFlags(FCVAR_GAMEDLL);
	}

	// big ol' try catch because game has a TerminateProcess handler for exceptions...
	// why this wasn't here is mystifying, - 10/2024 NULLderef
	try {
		// Byte patches
		P2MMLog(0, true, "Patching Portal 2...");

		// Linked portal doors event crash patch
		Memory::ReplacePattern("server", "0F B6 87 04 05 00 00 8B 16", "EB 14 87 04 05 00 00 8B 16");

		// Partner disconnects
		Memory::ReplacePattern("server", "51 50 FF D2 83 C4 10 E8", "51 50 90 90 83 C4 10 E8");
		Memory::ReplacePattern("server", "74 28 3B 75 FC", "EB 28 3B 75 FC");

		// Max players -> 33
		Memory::ReplacePattern("server", "83 C0 02 89 01", "83 C0 20 89 01");
		Memory::ReplacePattern("engine", "85 C0 78 13 8B 17", "31 C0 04 21 8B 17");
		uintptr_t svPtr = *reinterpret_cast<uintptr_t*>(Memory::Scanner::Scan<void*>(ENGINEDLL, "74 0A B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B E5", 3));
		*reinterpret_cast<int*>(svPtr + 0x228) = 33;

		// Store pointer to the CBaseServer for global access.
		this->sv = reinterpret_cast<CBaseServer*>(svPtr);

		// Prevent disconnect by "STEAM validation rejected"
		Memory::ReplacePattern("engine", "01 74 7D 8B", "01 EB 7D 8B");

		// Fix sv_password
		Memory::ReplacePattern("engine", "0F 95 C1 51 8D 4D E8", "03 C9 90 51 8D 4D E8");

		// runtime max 0.03 -> 0.05
		Memory::ReplacePattern("vscript", "00 00 00 E0 51 B8 9E 3F", "9a 99 99 99 99 99 a9 3f");

		// Make sure -allowspectators is there so we get our 33 max players
		if (!CommandLine()->FindParm("-allowspectators"))
		{
			CommandLine()->AppendParm("-allowspectators", "");
		}

		// MinHook initialization and hooking
		P2MMLog(0, true, "Initializing MinHook and hooking functions...");
		MH_Initialize();

		// NoSteamLogon disconnect hook patch.
		MH_CreateHook(
			(LPVOID)Memory::Scanner::Scan<void*>(ENGINEDLL, "55 8B EC 83 EC 08 53 56 57 8B F1 E8 ?? ?? ?? ?? 8B"),
			&CSteam3Server__OnGSClientDenyHelper_hook, (void**)&CSteam3Server__OnGSClientDenyHelper_orig
		);

		// Hook onto the function which defines what Atlas's and PBody's models are.
		MH_CreateHook(
			Memory::Rel32(Memory::Scanner::Scan(SERVERDLL, "E8 ?? ?? ?? ?? 83 C4 40 50", 1)),
			&GetBallBotModel_hook, (void**)&GetBallBotModel_orig
		);
		MH_CreateHook(
			Memory::Rel32(Memory::Scanner::Scan(SERVERDLL, "E8 ?? ?? ?? ?? 83 C4 04 50 8B 45 10 8B 10", 1)),
			&GetEggBotModel_hook, (void**)&GetEggBotModel_orig
		);
		MH_CreateHook(
			Memory::Scanner::Scan(SERVERDLL, "55 8B EC 81 EC 10 01 00 00 53 8B 1D"),
			&CPortal_Player__GetPlayerModelName_hook, (void**)&CPortal_Player__GetPlayerModelName_orig
		);

		// For p2mm_instant_respawn.
		MH_CreateHook(
			Memory::Scanner::Scan(SERVERDLL, "53 8B DC 83 EC 08 83 E4 F0 83 C4 04 55 8B 6B ?? 89 6C 24 ?? 8B EC A1 ?? ?? ?? ?? F3 0F 10 40 ?? F3 0F 58 05 ?? ?? ?? ?? 83 EC 28 56 57 6A 00 51 8B F1 F3 0F 11 04 24 E8 ?? ?? ?? ?? 6A 03"),
			&CPortal_Player__PlayerDeathThink_hook, (void**)&CPortal_Player__PlayerDeathThink_orig
		);

		// "respawn" function hook for getting a VScript "game event" call out of it.
		MH_CreateHook(
			Memory::Scanner::Scan(SERVERDLL, "55 8B EC A1 ?? ?? ?? ?? 80 78 ?? ?? 75 ?? 80 78"),
			&respawn_hook, (void**)&respawn_orig
		);

		MH_EnableHook(MH_ALL_HOOKS);
	} catch (const std::exception& ex) {
		P2MMLog(0, false, "Failed to load plugin! :( Exception: \"%s\"", ex.what());
		this->m_bNoUnload = true;
		return false;
	}

	g_pDiscordIntegration->UpdateDiscordRPC();

	P2MMLog(0, false, "Loaded plugin! Horray! :D");
	m_bPluginLoaded = true;
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is turning off/unloading.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::Unload(void)
{
	// If the plugin errors for some reason, prevent it from unloading.
	if (m_bNoUnload)
	{
		m_bNoUnload = false;
		return;
	}

	P2MMLog(0, false, "Unloading Plugin...");
	this->m_bPluginUnloading = true;
	g_pDiscordIntegration->UpdateDiscordRPC();

	P2MMLog(0, true, "Removing listeners for game events...");
	g_pGameEventManager->RemoveListener(this);

	// Unblock ConCommands that clients shouldn't execute
	P2MMLog(0, true, "Unblocking console commands...");
	for (const char* concommand : forbiddenConCommands)
	{
		ConCommandBase* commandbase = g_pCVar->FindCommandBase(concommand);
		if (commandbase)
			commandbase->AddFlags(FCVAR_GAMEDLL);
	}

	ConVar_Unregister();
	P2MMLog(0, true, "Disconnecting tier libraries...");
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();

	// Undo byte patches
	P2MMLog(0, true, "Unpatching Portal 2...");

	// Linked portal doors event crash patch
	Memory::ReplacePattern("server", "EB 14 87 04 05 00 00 8B 16", "0F B6 87 04 05 00 00 8B 16");

	// Partner disconnects
	Memory::ReplacePattern("server", "51 50 90 90 83 C4 10 E8", "51 50 FF D2 83 C4 10 E8");
	Memory::ReplacePattern("server", "EB 28 3B 75 FC", "74 28 3B 75 FC");

	// Max players -> 3
	Memory::ReplacePattern("server", "83 C0 20 89 01", "83 C0 02 89 01");
	Memory::ReplacePattern("engine", "31 C0 04 21 8B 17", "85 C0 78 13 8B 17");
	*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this->sv) + 0x228) = 2;

	// Disconnect by "STEAM validation rejected"
	Memory::ReplacePattern("engine", "01 EB 7D 8B", "01 74 7D 8B");

	// sv_password
	Memory::ReplacePattern("engine", "03 C9 90 51 8D 4D E8", "0F 95 C1 51 8D 4D E8");

	// runtime max 0.05 -> 0.03
	Memory::ReplacePattern("vscript", "00 00 00 00 00 00 E0 3F", "00 00 00 E0 51 B8 9E 3F");

	P2MMLog(0, true, "Disconnecting hooked functions and uninitializing MinHook...");
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();

	if (p2mm_discord_rpc.GetBool() && g_pDiscordIntegration->RPCRunning)
		g_pDiscordIntegration->ShutdownDiscordRPC();

	m_bPluginLoaded = false;
	P2MMLog(0, false, "Plugin unloaded! Goodbye!");
}

//---------------------------------------------------------------------------------
// Purpose: For ClientCommand.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::SetCommandClient(int index)
{
	m_iClientCommandIndex = index;
}

//---------------------------------------------------------------------------------
// Purpose: When the server activates, start the P2:MM VScript.
//---------------------------------------------------------------------------------
void RegisterFuncsAndRun();
void CP2MMServerPlugin::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	RegisterFuncsAndRun();
}

//---------------------------------------------------------------------------------
// Purpose: Called when a map has started loading.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::LevelInit(char const* pMapName)
{
	P2MMLog(0, true, "Level Init!");

	// Dedicated server paint map patch
	// Paint usage doesn't function naturally on dedicated servers, so this will help enable it again.
	if (p2mm_ds_enable_paint.GetBool() && engineServer->IsDedicatedServer())
	{
		// Hook R_LoadWorldGeometry (gl_rmisc.cpp)
		static auto R_LoadWorldGeometry =
#ifdef _WIN32
			reinterpret_cast<void(__cdecl*)(bool bDXChange)>(Memory::Scanner::Scan<void*>(ENGINEDLL, "55 8B EC 83 EC 14 53 33 DB 89"));
#else
			NULL; // TODO: Linux & MacOS
#endif //  _WIN32
		if (R_LoadWorldGeometry)
		{
			CUtlVector< uint32 > paintData;
			engineServer->GetPaintmapDataRLE(paintData);
			R_LoadWorldGeometry(false);
			CUtlVector< uint32 > paintData2;
			engineServer->GetPaintmapDataRLE(paintData2);
		}
		else
			P2MMLog(1, false, "Couldn't find R_LoadWorldGeometry! Paint will not work on this map load.");
	}

	if (!g_P2MMServerPlugin.m_bSeenFirstRunPrompt) return;

	std::string changemapstr = std::string("The server has changed the map to: `" + std::string(CURMAPFILENAME) + "`");
	g_pDiscordIntegration->SendWebHookEmbed("Server", changemapstr, EMBEDCOLOR_SERVER, false);

	// Update Discord RPC to update current map information.
	g_pDiscordIntegration->UpdateDiscordRPC();
}

//---------------------------------------------------------------------------------
// Purpose: Called when a client inputs a console command.
//---------------------------------------------------------------------------------
PLUGIN_RESULT CP2MMServerPlugin::ClientCommand(edict_t* pEntity, const CCommand& args)
{
	// Check if its a valid player edict.
	if (!pEntity || pEntity->IsFree())
		return PLUGIN_CONTINUE;

	const char* pcmd = args[0];
	const char* fargs = args.ArgS();

	int userid = engineServer->GetPlayerUserId(pEntity);
	int entindex = UserIDToPlayerIndex(userid);
	const char* playername = GetPlayerName(entindex);

	if (p2mm_spewgameeventinfo.GetBool())
	{
		P2MMLog(0, true, "ClientCommand called: %s", pcmd);
		P2MMLog(0, true, "ClientCommand args: %s", fargs);
		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "entindex: %i", entindex);
		P2MMLog(0, true, "playername: %s", playername);
		P2MMLog(0, true, "VScript VM Working?: %s", (g_pScriptVM != NULL) ? "Working" : "Not Working!");
	}

	// Call the "GEClientCommand" VScript function
	if (g_pScriptVM)
	{
		HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEClientCommand");
		if (ge_func)
			g_pScriptVM->Call<const char*, const char*, int, int, const char*>(ge_func, NULL, false, NULL, pcmd, fargs, userid, entindex, playername);
	}

	// signify is the client command used to make on screen icons appear
	if (FStrEq(pcmd, "signify"))
	{
		// Check if its the death icons and if the death icons disable ConVar is on
		if ((FStrEq(args[1], "death_blue") || FStrEq(args[1], "death_orange")) && !p2mm_deathicons.GetBool())
			return PLUGIN_STOP;
	}

	// Stop certain client commands from being excecated by clients and not the host
	for (const char* badcc : forbiddenClientCommands)
	{
		// These commands can be manually called to make everyone emote,
		// however there are certain other ones we need to let in for players individually to emote.
		if (FSubStr(pcmd, "taunt_auto") || FSubStr(pcmd, "mp_earn_taunt"))
			return PLUGIN_STOP;

		// Whether we want to actually stop client commands or not. Host is always ignored.
		if (entindex != 1 && FSubStr(pcmd, badcc) && p2mm_forbidclientcommands.GetBool())
		{
			engineServer->ClientPrintf(INDEXENT(entindex), "This command is blocked from execution!\n");
			return PLUGIN_STOP;
		}
	}

	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: Capture and work with game events. Interfaces game events to VScript functions.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::FireGameEvent(IGameEvent* event)
{
	bool spewinfo = p2mm_spewgameeventinfo.GetBool();
	if (spewinfo)
	{
		P2MMLog(0, true, "Game Event Fired: %s", event->GetName());
		P2MMLog(0, true, "VScript VM Working?: %s", (g_pScriptVM != NULL) ? "Working" : "Not Working!");
	}

	// Event called when a player pings, "portal_player_ping" returns:
	/*
		"userid"	"short"		// user ID on server
		"ping_x"	"float"		// ping's x-coordinate in map
		"ping_y"	"float"		// ping's y-coordinate in map
		"ping_z"	"float"		// ping's z-coordinate in map
	*/
	if (FStrEq(event->GetName(), "portal_player_ping"))
	{
		short userid = event->GetInt("userid");
		float ping_x = event->GetFloat("ping_x");
		float ping_y = event->GetFloat("ping_y");
		float ping_z = event->GetFloat("ping_z");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerPing");
			if (ge_func)
				g_pScriptVM->Call<short, float, float, float, int>(ge_func, NULL, false, NULL, userid, ping_x, ping_y, ping_z, entindex);
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "ping_x: %f", ping_x);
			P2MMLog(0, true, "ping_y: %f", ping_y);
			P2MMLog(0, true, "ping_z: %f", ping_z);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player goes through a portal, "portal_player_portaled" returns:
	/*
		"userid"	"short"		// user ID on server
		"portal2"	"bool"		// false for portal1 (blue)
	*/
	else if (FStrEq(event->GetName(), "portal_player_portaled"))
	{
		short userid = event->GetInt("userid");
		bool portal2 = event->GetString("text");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerPortaled");
			if (ge_func)
				g_pScriptVM->Call<short, bool, int>(ge_func, NULL, false, NULL, userid, portal2, entindex);
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "portal2: %s", portal2 ? "true" : "false");
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a turret hits another turret, "turret_hit_turret" returns nothing.
	else if (FStrEq(event->GetName(), "turret_hit_turret"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GETurretHitTurret");
			if (ge_func)
				g_pScriptVM->Call(ge_func, NULL, false, NULL);
		}

		return;
	}
	// Event called when a camera is detached from a wall, "security_camera_detached" returns nothing.
	else if (FStrEq(event->GetName(), "security_camera_detached"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GECamDetach");
			if (ge_func)
				g_pScriptVM->Call(ge_func, NULL, false, NULL);
		}

		return;
	}
	// Event called when a player touches the ground, "player_landed" returns:	
	/*
		"userid"	"short"		// user ID on server
	*/
	else if (FStrEq(event->GetName(), "player_landed"))
	{
		short userid = event->GetInt("userid");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerLanded");
			if (ge_func)
				g_pScriptVM->Call<short, int>(ge_func, NULL, false, NULL, userid, entindex);
		}

		return;
	}
	// Event called when a Blue/Atlas spawns, "player_spawn_blue" returns nothing.
	else if (FStrEq(event->GetName(), "player_spawn_blue"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSpawnBlue");
			if (ge_func)
				g_pScriptVM->Call(ge_func, NULL, false, NULL);
		}

		return;
	}
	// Event called when a Red/Orange/PBody spawns, "player_spawn_orange" returns nothing.
	else if (FStrEq(event->GetName(), "player_spawn_orange"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSpawnOrange");
			if (ge_func)
				g_pScriptVM->Call(ge_func, NULL, false, NULL);
		}

		return;
	}
	// Event called when a player dies, "player_death" returns:	
	/*
		"userid"	"short"   	// user ID who died
		"attacker"	"short"	 	// user ID who killed
	*/
	else if (FStrEq(event->GetName(), "player_death"))
	{
		short userid = event->GetInt("userid");
		short attacker = event->GetInt("attacker");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handling OnDeath VScript event
			HSCRIPT od_func = g_pScriptVM->LookupFunction("OnDeath");
			if (od_func)
			{
				HSCRIPT playerHandle = INDEXHANDLE(entindex);
				if (playerHandle)
				{
					g_pScriptVM->Call<HSCRIPT>(od_func, NULL, false, NULL, playerHandle);
					g_pDiscordIntegration->SendWebHookEmbed(std::string(GetPlayerName(entindex) + std::string(" Died!")), "", EMBEDCOLOR_PLAYERDEATH);
				}
			}

			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerDeath");
			if (ge_func)
				g_pScriptVM->Call<short, short, int>(ge_func, NULL, false, NULL, userid, attacker, entindex);
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "attacker: %i", attacker);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player spawns, "player_spawn" returns:	
	/*
		"userid"	"short"		// user ID on server
	*/
	else if (FStrEq(event->GetName(), "player_spawn"))
	{
		short userid = event->GetInt("userid");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSpawn");
			if (ge_func)
				g_pScriptVM->Call<short, int>(ge_func, NULL, false, NULL, userid, entindex);
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// The game event for player's connecting is used instead of the plugin's ClientFullyConnected
	// callback because the game event gives more information than the callback without having to do
	// any extra work to get information.
	// Event called when a player connects to the server, "player_connect" returns:
	/*
		"name"		"string"	// player name
		"index"		"byte"		// player slot (entity index-1)
		"userid"	"short"		// user ID on server (unique on server) "STEAM_1:...", will be "BOT" if player is bot
		"xuid"		"uint64"	// XUID/Steam ID (converted to const char*)
		"networkid" "string" 	// player network (i.e steam) id
		"address"	"string"	// ip:port
		"bot"		"bool"		// player is a bot
	}
	*/
	else if (FStrEq(event->GetName(), "player_connect"))
	{
		const char* name = event->GetString("name");
		int index = event->GetInt("index");
		short userid = event->GetInt("userid");
		const char* xuid = std::to_string(event->GetUint64("xuid")).c_str();
		const char* networkid = event->GetString("networkid");
		const char* address = event->GetString("address");
		bool bot = event->GetBool("bot");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerConnect");
			if (ge_func)
			{
				g_pScriptVM->Call<const char*, int, short, const char*, const char*, const char*, bool, int>(ge_func, NULL, false, NULL, name, index, userid, xuid, networkid, address, bot, entindex);
				g_pDiscordIntegration->SendWebHookEmbed(std::string(name + std::string(" Joinned!")), std::string(name + std::string(" joinned the server!")));
			}
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "name: %s", name);
			P2MMLog(0, true, "index: %i", index);
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "xuid: %d", xuid);
			P2MMLog(0, true, "networkid: %s", networkid);
			P2MMLog(0, true, "address: %s", address);
			P2MMLog(0, true, "bot: %i", bot);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player changes their name, "player_info" returns:
	/*
		"name"		"string"	// player name
		"index"		"byte"		// player slot (entity index-1)
		"userid"	"short"		// user ID on server (unique on server) "STEAM_1:...", will be "BOT" if player is bot
		"friendsid" "short"		// friends identification number
		"networkid"	"string"	// player network (i.e steam) id
		"bot"		"bool"		// true if player is a AI bot
	*/
	else if (FStrEq(event->GetName(), "player_info"))
	{
		const char* name = event->GetString("name");
		int index = event->GetInt("index");
		short userid = event->GetInt("userid");
		const char* networkid = event->GetString("networkid");
		const char* address = event->GetString("address");
		bool bot = event->GetBool("bot");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerInfo");
			if (ge_func)
				g_pScriptVM->Call<const char*, int, short, const char*, const char*, bool, int>(ge_func, NULL, false, NULL, name, index, userid, networkid, address, bot, entindex);
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "name: %s", name);
			P2MMLog(0, true, "index: %i", index);
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "networkid: %s", networkid);
			P2MMLog(0, true, "address: %s", address);
			P2MMLog(0, true, "bot: %i", bot);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player inputs a message into the chat, "player_say" returns:
	/*
		"userid"	"short"		// user ID on server
		"text"		"string"	// the say text
	*/
	else if (FStrEq(event->GetName(), "player_say"))
	{
		short userid = event->GetInt("userid");
		const char* text = event->GetString("text");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			if (entindex != NULL)
			{
				// Handling chat commands
				HSCRIPT cc_func = g_pScriptVM->LookupFunction("ChatCommands");
				if (cc_func)
				{
					g_pScriptVM->Call<const char*, int>(cc_func, NULL, false, NULL, text, entindex);

					std::string playerName = GetPlayerName(entindex);
					std::string chatMsg = text;

					P2MMLog(0, true, playerName.c_str());
					P2MMLog(0, true, chatMsg.c_str());

					// Replace any "\\" characters with "\\\\" so backslashes can exist but not break anything
					size_t pos = 0;
					while ((pos = playerName.find("\\", pos)) != std::string::npos) {
						playerName.replace(pos, 1, std::string("\\\\"));
						pos += std::string("\\\\").length();
					}
					pos = 0;
					while ((pos = chatMsg.find("\\", pos)) != std::string::npos) {
						chatMsg.replace(pos, 1, std::string("\\\\"));
						pos += std::string("\\\\").length();
					}

					g_pDiscordIntegration->SendWebHookEmbed(playerName, chatMsg);
				}
			}

			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSay");
			if (ge_func)
				g_pScriptVM->Call<short, const char*, int>(ge_func, NULL, false, NULL, userid, text, entindex);
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "text: %s", text);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}

	return;
}

//---------------------------------------------------------------------------------
// Purpose: Called when a player is "activated" in the server, meaning fully loaded, not fully connect which happens before that.
// Called when game event "player_activate" is also called so this is used to call "GEClientActive".
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::ClientActive(edict_t* pEntity)
{
	short userid = engineServer->GetPlayerUserId(pEntity);
	int entindex = UserIDToPlayerIndex(userid);

	if (p2mm_spewgameeventinfo.GetBool())
	{
		P2MMLog(0, true, "ClientActive Called!");
		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "entindex: %i", entindex);
	}

	// Make sure people know that the chat is being recorded if webhook is set
	if (p2mm_discord_webhooks.GetBool())
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(entindex);
		if (pPlayer)
		{
			P2MMLog(0, true, "Warning for enabled webhooks sent to player index %i.", entindex);
			UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, "This lobby has Discord Webhook Intergration enabled. All of your ingame messages may be sent to a Discord channel.");
		}
	}

	if (g_pScriptVM)
	{
		// Handling OnPlayerJoin VScript event
		HSCRIPT opj_func = g_pScriptVM->LookupFunction("OnPlayerJoin");
		if (opj_func)
		{
			HSCRIPT playerHandle = INDEXHANDLE(entindex);
			if (playerHandle)
				g_pScriptVM->Call<HSCRIPT>(opj_func, NULL, false, NULL, playerHandle);
		}

		// Handle VScript game event function
		HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEClientActive");
		if (ge_func)
			g_pScriptVM->Call<short, int>(ge_func, NULL, false, NULL, userid, entindex);
	}

	// Update Discord RPC to update player count.
	g_pDiscordIntegration->UpdateDiscordRPC();
	return;
}

//---------------------------------------------------------------------------------
// Purpose: Called every server frame, used for the VScript loop. Warning: Don't do too intensive tasks with this!
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::GameFrame(bool simulating)
{
	HSCRIPT loop_func = g_pScriptVM->LookupFunction("P2MMLoop");
	if (loop_func && p2mm_loop.GetBool())
		g_pScriptVM->Call(loop_func, NULL, false, NULL);

	// Handle VScript game event function
	HSCRIPT gf_func = g_pScriptVM->LookupFunction("GEGameFrame");
	if (gf_func)
		g_pScriptVM->Call<bool>(gf_func, NULL, false, NULL, simulating);
}

//---------------------------------------------------------------------------------
// Purpose: Called when a the map is changing to another map, or the server is shutting down.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::LevelShutdown(void)
{
	P2MMLog(0, true, "Level Shutdown!");
	p2mm_loop.SetValue("0"); // REMOVE THIS at some point...
	updateMapsList(); // Update the maps list for p2mm_map.
	// Update Discord RPC to update the level information or to say the host is on the main menu.
	g_pDiscordIntegration->UpdateDiscordRPC();
}

//---------------------------------------------------------------------------------
// Purpose: Unused callbacks
//---------------------------------------------------------------------------------
#pragma region UNUSED_CALLBACKS
void CP2MMServerPlugin::Pause(void) {}
void CP2MMServerPlugin::UnPause(void) {}
void CP2MMServerPlugin::ClientDisconnect(edict_t* pEntity) {}
void CP2MMServerPlugin::ClientFullyConnect(edict_t* pEntity) {} // Purpose: Called when a player is fully connected to the server. Player entity still has not spawned in so manipulation is not possible.
void CP2MMServerPlugin::ClientPutInServer(edict_t* pEntity, char const* playername) {}
void CP2MMServerPlugin::ClientSettingsChanged(edict_t* pEdict) {}
PLUGIN_RESULT CP2MMServerPlugin::ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) { return PLUGIN_CONTINUE; }
PLUGIN_RESULT CP2MMServerPlugin::NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) {}
void CP2MMServerPlugin::OnEdictAllocated(edict_t* edict) {}
void CP2MMServerPlugin::OnEdictFreed(const edict_t* edict) {}
bool CP2MMServerPlugin::BNetworkCryptKeyCheckRequired(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey) { return false; }
bool CP2MMServerPlugin::BNetworkCryptKeyValidate(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte* pbEncryptedBufferFromClient, byte* pbPlainTextKeyForNetchan) { return true; }
#pragma endregion