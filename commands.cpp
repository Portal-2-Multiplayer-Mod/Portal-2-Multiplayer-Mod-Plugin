//===========================================================================//
//
// Author: Orsell
// Purpose: Where ConVars and ConCommands are defined and used throughout the plugin.
// 
//===========================================================================//
#include "commands.hpp"

#include "globals.hpp"
#include "sdk.hpp"
#include "p2mm.hpp"
#include "discordrpc.hpp"

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
ConVar p2mm_instantrespawn("p2mm_instantrespawn", "0", FCVAR_NONE, "Whether respawning should be instant or not.");

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
// P2:MM p2mm_map ConCommand Logic
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
		g_pDiscordIntegration->SendWebHookEmbed("Server", initmapstr, EMBEDCOLOR_SERVER, false);
	}
	else
	{
		P2MMLog(0, true, "\"mp_coop\" found, multiplayer map being run. Full ExecuteClientCmd: \"%s\"", std::string(mapString + requestedMap).c_str());
		P2MMLog(0, true, "requestedMap: \"%s\"", requestedMap);
		engineClient->ExecuteClientCmd(std::string(mapString + requestedMap).c_str());

		std::string initmapstr = std::string("Server has started with map: `" + std::string(requestedMap) + "`");
		g_pDiscordIntegration->SendWebHookEmbed("Server", initmapstr, EMBEDCOLOR_SERVER, false);
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
	HudMessageParams helloWorldParams;
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
	HudMessageParams lapMessage;
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
	HudMessageParams musicMessage;
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