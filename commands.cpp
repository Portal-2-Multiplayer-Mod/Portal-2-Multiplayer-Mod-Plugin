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
ConVar p2mm_loop("p2mm_loop", "0", FCVAR_HIDDEN, "Flag if P2MMLoop should be looping."); //! REMOVE THIS AT SOME POINT!!!
ConVar p2mm_lastmap("p2mm_lastmap", "", FCVAR_HIDDEN, "Last map recorded for the Last Map system.");
ConVar p2mm_splitscreen("p2mm_splitscreen", "0", FCVAR_HIDDEN, "Flag for the main menu buttons and launcher to start in splitscreen or not.");

//---------------------------------------------------------------------------------
// UTIL P2:MM ConVars | ConVars the host can change.
//---------------------------------------------------------------------------------
ConVar p2mm_forbid_clientcommands("p2mm_forbid_clientcommands", "1", FCVAR_NONE, "Stop client commands clients shouldn't be executing.");
ConVar p2mm_deathicons("p2mm_deathicons", "1", FCVAR_NONE, "Whether or not when players die the death icon should appear.");
ConVar p2mm_instantrespawn("p2mm_instantrespawn", "0", FCVAR_NONE, "Whether respawning should be instant or not.");

//---------------------------------------------------------------------------------
// Debug P2:MM ConVars | Self-explanatory.
//---------------------------------------------------------------------------------
ConVar p2mm_developer("p2mm_developer", "0", FCVAR_NONE, "Enable for P2:MM developer messages.");

static void UpdateDisplayGEsConVar(IConVar* var, const char* pOldValue, float flOldValue)
{
	if (ConVar* pGEConVar = g_pCVar->FindVar("display_game_events"))
		pGEConVar->SetValue(dynamic_cast<ConVar*>(var)->GetBool());
}
ConVar p2mm_spew_gameevent_info("p2mm_spew_gameevent_info", "0", FCVAR_NONE, "Log information from called game events in the console, p2mm_developer must also be on. Can cause lots of console spam.", UpdateDisplayGEsConVar);

//---------------------------------------------------------------------------------
// P2:MM p2mm_map ConCommand Logic
//---------------------------------------------------------------------------------
static std::vector<std::string> mapList; // List of maps for the p2mm_map command auto complete.
static std::vector<std::string> workshopMapList; // List of all workshop map for the p2mm_map auto complete.

// Update the map list available to p2mm_map by scanning for all map files in SearchPath.
void UpdateMapsList()
{
	mapList.clear();
	CUtlVector<CUtlString> outList;
	AddFilesToList(outList, "maps", "GAME", "bsp");

	FOR_EACH_VEC(outList, i)
	{
		// Get each map and get their relative path to each SearchPath and make slashes forward slashes.
		// Then turn relativePath into a std::string to easily manipulate.
		const char* curMap = outList[i];
		char relativePath[MAX_PATH] = { 0 };
		g_pFileSystem->FullPathToRelativePathEx(curMap, "GAME", relativePath, sizeof(relativePath));
		V_FixSlashes(relativePath, '/');
		V_StripExtension(relativePath, relativePath, sizeof(relativePath));
		std::string fixedRelativePath(relativePath);

		// Remove "maps/" out of the string.
		fixedRelativePath.erase(0, strlen("maps/"));

		// Remove the whole "workshop/(workshop id)" part if there isn't multiple workshop maps of the same file name.
		if (const size_t lastSlashPos = fixedRelativePath.find_last_of('/'); lastSlashPos != std::string::npos && fixedRelativePath.rfind("workshop/") != std::string::npos)
		{
			fixedRelativePath.erase(0, strlen("workshop/"));
			workshopMapList.push_back(fixedRelativePath); // Save workshop maps onto a separate list for checking in p2mm_map.
		}

		// Push the map string on to the list to display available options for the command.
		mapList.push_back(std::move(fixedRelativePath));
	}
}

// Autocomplete for p2mm_map.
static int p2mm_map_CompletionFunc(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	// If the map list is empty, generate it.
	if (mapList.empty())
		UpdateMapsList();

	// Assemble together the current state of the inputted command.
	const auto conCommand = "p2mm_map ";
	const char* match = (V_strstr(partial, conCommand) == partial) ? partial + V_strlen(conCommand) : partial;

	// Go through the map list searching for matches with the assembled inputted command.
	int numMatchedMaps = 0;
	for (const std::string& map : mapList)
	{
		if (numMatchedMaps >= COMMAND_COMPLETION_MAXITEMS) break;

		if (V_strstr(map.c_str(), match))
		{
			V_snprintf(commands[numMatchedMaps++], COMMAND_COMPLETION_ITEM_LENGTH, "%s%s", conCommand, map.c_str());
		}
	}

	return numMatchedMaps;
}

CON_COMMAND_F_COMPLETION(p2mm_map, "Starts up a P2:MM session with a requested map.", FCVAR_NONE, p2mm_map_CompletionFunc)
{
	// If the map list is empty, generate it.
	if (mapList.empty())
		UpdateMapsList();

	// Make sure the CONCOMMAND was executed correctly.
	if (args.ArgC() < 2 || FStrEq(args.Arg(1), ""))
	{
		Log(WARNING, false, "p2mm_map called incorrectly! Usage: \"p2mm_map (map to start)\"");
		UpdateMapsList();
		return;
	}

	// A check done by the menu to request to use the last recorded map in the p2mm_lastmap ConVar.
	char requestedMap[256] = { 0 };
	V_strcpy(requestedMap, args.Arg(1));
	Log(INFO, true, "Requested Map: %s", requestedMap);
	Log(INFO, true, "p2mm_lastmap: %s", p2mm_lastmap.GetString());
	if (FStrEq(requestedMap, "P2MM_LASTMAP"))
	{
		if (!engineServer->IsMapValid(p2mm_lastmap.GetString()))
		{
			// Running disconnect to make the error screen appear causes the music to stop, don't let that to happen, so here it is started it again.

			// Get the current act so we can start the right main menu music.
			int iAct = ConVarRef("ui_lastact_played").GetInt();
			if (iAct > 5)
				iAct = 5;
			else if (iAct < 1)
				iAct = 1;

			// Put the command to start the music and the act number together.
			char completePvCmd[sizeof("playvol \"#music/mainmenu/portal2_background0%d\" 0.35") + sizeof(iAct)] = { 0 };
			V_snprintf(completePvCmd, sizeof(completePvCmd), R"(playvol "#music/mainmenu/portal2_background0%i" 0.35)", iAct);

			Log(WARNING, false, "p2mm_map was called with P2MM_LASTMAP, but p2mm_lastmap is empty or invalid!");
			engineClient->ExecuteClientCmd("disconnect \"There is no last map recorded or the map doesn't exist! Please start a play session with the other options first.\"");
			engineClient->ExecuteClientCmd(completePvCmd);
			UpdateMapsList();
			return;
		}
		
		V_strcpy(requestedMap, p2mm_lastmap.GetString());
		Log(INFO, true, "P2MM_LASTMAP called! Running Last Map: \"%s\"", requestedMap);
	}
	p2mm_lastmap.SetValue(""); // Set last map ConVar to blank so it doesn't trigger level changes where we don't want it to trigger.

	// Check if the requested map is a workshop map.
	const std::string tempMapStr = requestedMap;
	for (const std::string& map : workshopMapList)
	{
		if (tempMapStr == map)
			V_strcpy(requestedMap, std::string("workshop/" + tempMapStr).c_str());
	}

	// Check if the supplied map is a valid map.
	if (!engineServer->IsMapValid(requestedMap))
	{
		Log(WARNING, false, "p2mm_map was given a non-valid map or one that doesn't exist! \"%s\"", requestedMap);
		UpdateMapsList();
		return;
	}

	// Check if the user requested it to start in splitscreen or not.
	const std::string mapString = p2mm_splitscreen.GetBool() ? "ss_map " : "map ";
	Log(INFO, true, "Map String: %s", mapString.c_str());
	
	g_P2MMServerPlugin.m_bFirstMapRan = true;
	g_P2MMServerPlugin.m_bSeenFirstRunPrompt = false;

	// Load the map and send webhook.
	engineClient->ExecuteClientCmd(std::string(mapString + requestedMap + " *mp").c_str());
	const auto initMapStr = std::string("Server has started with map: `" + std::string(requestedMap) + "`");
	CDiscordIntegration::SendWebHookEmbed("Server", initMapStr, EMBED_COLOR_SERVER, false);
}

CON_COMMAND_F(p2mm_updatemaplist, "Manually updates the list of available maps that can be loaded with p2mm_map.", FCVAR_HIDDEN)
{
	UpdateMapsList();
}

CON_COMMAND_F(p2mm_maplist, "Lists available maps that can be loaded with p2mm_map.", FCVAR_HIDDEN)
{
	Log(INFO, false, "AVAILABLE MAPS:");
	Log(INFO, false, "----------------------------------------");
	for (const std::string& map : mapList)
		Log(INFO, false, map.c_str());
	Log(INFO, false, "----------------------------------------");
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

static bool cvccShown = false; // Bool to track if the hidden ConVars and ConCommands are showing.
static std::vector<ConCommandBase*> toggledCVCCs; // List of toggled ConVars and ConCommands with the FCVAR_DEVELOPMENTONLY and FCVAR_HIDDEN ConVar flags removed.
CON_COMMAND_F(p2mm_toggle_dev_cc_cvars, "Toggle showing any ConVars and ConCommands that have the FCVAR_DEVELOPMENTONLY and FCVAR_HIDDEN ConVar flags.", FCVAR_HIDDEN)
{
	int iToggleCount = 0; // To tell the user how many ConVars and ConCommands where toggle to show or hide.

	if (cvccShown)
	{
		// Hide the ConVars and ConCommands
		for (ConCommandBase* pCommandVarName : toggledCVCCs)
		{
			pCommandVarName->AddFlags(FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN);
			iToggleCount++;
		}
		toggledCVCCs.clear();
		cvccShown = false;
	}
	else
	{
		// Remove development and hidden flags from the ConVars and ConCommands
		FOR_ALL_CONSOLE_COMMANDS(pCommandVarName)
		{
			if (pCommandVarName->IsFlagSet(FCVAR_DEVELOPMENTONLY) || pCommandVarName->IsFlagSet(FCVAR_HIDDEN))
			{
				pCommandVarName->RemoveFlags(FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN);
				iToggleCount++;
				toggledCVCCs.push_back(pCommandVarName);
			}
		}
		cvccShown = true;
	}

	Log(INFO, false, "%s %i ConVars/ConCommands!", cvccShown ? "Unhid" : "Hid", iToggleCount);
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

static void GelocityTournament(IConVar* var, const char* pOldValue, const float flOldValue)
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		Log(INFO, false, "Gelocity tournament mode ConVar was changed from %i to %i.", static_cast<int>(flOldValue), dynamic_cast<ConVar*>(var)->GetBool());
		Log(WARNING, false, "Mode will take effect when Gelocity map is loaded.");
		return;
	}

	// Check if the gelocity race is already going. Make sure to not mess with the race's laps.
	ScriptVariant_t raceStartedScript;
	g_pScriptVM->GetValue("b_RaceStarted", &raceStartedScript);
	if (raceStartedScript.m_bool)
	{
		Log(WARNING, false, "Race is currently in progress!");
		return;
	}

	Log(INFO, false, "Gelocity tournament mode ConVar was changed from %i to %i!", static_cast<int>(flOldValue), dynamic_cast<ConVar*>(var)->GetBool());
	Log(WARNING, false, "Restarting map based on tournament mode change!");

	engineClient->ExecuteClientCmd(std::string("changelevel " + std::string(CUR_MAPFILE_NAME)).c_str());
}
ConVar p2mm_gelocity_tournamentmode("p2mm_gelocity_tournamentmode", "0", FCVAR_NONE, "Turn on or off tournament mode.", true, 0, true, 1, GelocityTournament);

static void GelocityButtons(IConVar* var, const char* pOldValue, float flOldValue)
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		if (!dynamic_cast<ConVar*>(var)->GetBool())
			Log(INFO, false, "Unlocked buttons...");
		else
			Log(INFO, false, "Locked buttons...");
		Log(WARNING, false, "Mode will take effect when Gelocity map is loaded.");
		return;
	}

	// Lock or unlock the buttons.
	if (!dynamic_cast<ConVar*>(var)->GetBool())
	{
		g_pScriptVM->Run(
			"EntFire(\"rounds_button_1\", \"Unlock\");"
			"EntFire(\"rounds_button_2\", \"Unlock\");"
			"EntFire(\"music_button_1\", \"Unlock\");"
			"EntFire(\"music_button_2\", \"Unlock\");", false
		);
		Log(INFO, false, "Unlocked buttons...");
	}
	else
	{
		g_pScriptVM->Run(
			"EntFire(\"rounds_button_1\", \"Lock\");"
			"EntFire(\"rounds_button_2\", \"Lock\");"
			"EntFire(\"music_button_1\", \"Lock\");"
			"EntFire(\"music_button_2\", \"Lock\");", false
		);
		Log(INFO, false, "Locked buttons...");
	}
}
ConVar p2mm_gelocity_lockbuttons("p2mm_gelocity_lockbuttons", "0", FCVAR_NONE, "Toggle the state of the music and lap buttons.", true, 0, true, 1, GelocityButtons);

CON_COMMAND(p2mm_gelocity_laps, "Set lap count for the Gelocity Race. Specify 0 or no argument to see current lap count.")
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		Log(WARNING, false, "Not currently in a Gelocity map!");
		return;
	}

	// Check if the gelocity race is already going. Make sure to not mess with the race's laps.
	ScriptVariant_t raceStartedScript;
	g_pScriptVM->GetValue("bRaceStarted", &raceStartedScript);
	if (raceStartedScript.m_bool)
	{
		Log(WARNING, false, "Race is currently in progress!");
		return;
	}

	// Check if 0 or no arguments are specified so that the ConCommand
	// returns how many laps are currently set.
	// But if it's a value out of range, return error.
	if (V_atoi(args.Arg(1)) == 0 || args.ArgC() == 1)
	{
		ScriptVariant_t raceLaps;
		g_pScriptVM->GetValue("iGameLaps", &raceLaps);
		Log(INFO, false, "Current race laps: %i", raceLaps.m_int);
		return;
	}
	else if (V_atoi(args.Arg(1)) < 1 || V_atoi(args.Arg(1)) > 300)
	{
		Log(WARNING, false, "Value out of bounds! Lap counter goes from 1-300!");
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

	UTIL_HudMessage(nullptr, lapMessage, std::string("Race Laps: " + std::string(args.Arg(1))).c_str());
}

CON_COMMAND(p2mm_gelocity_music, "Set the music track for the Gelocity Race. 0-5 0 = No Music.")
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		Log(WARNING, false, "Not currently in a Gelocity map!");
		return;
	}

	// Check if the final lap has been triggered. LET THE INTENSE FINAL LAP MUSIC PLAY!
	ScriptVariant_t finalLapScript;
	g_pScriptVM->GetValue("bFinalLap", &finalLapScript);
	if (finalLapScript.m_bool)
	{
		Log(WARNING, false, "ITS THE FINAL LAP! LET THE INTENSE FINAL LAP MUSIC PLAY!");
		return;
	}

	// Check if value is out of bounds.
	if (args.ArgC() == 1)
	{
		ScriptVariant_t iMusicTrack;
		g_pScriptVM->GetValue("iMusicTrack", &iMusicTrack);
		Log(INFO, false, "Current music track: %i", iMusicTrack.m_int);
		return;
	}
	else if (V_atoi(args.Arg(1)) < 0 || V_atoi(args.Arg(1)) > 5)
	{
		Log(WARNING, false, "Value out of bounds! Music tracks goes from 0-5!");
		return;
	}

	g_pScriptVM->Run(std::string("iMusicTrack <- " + std::to_string(V_atoi(args.Arg(1))) + R"(; EntFire("counter_music", "SetValue", iMusicTrack.tostring());)").c_str(), false);
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
		Log(INFO, false, "Music turned off!", V_atoi(args.Arg(1)));
	}
	else
	{
		UTIL_HudMessage(NULL, musicMessage, std::string("Music Track: " + std::string(args.Arg(1))).c_str());
		Log(INFO, false, "Set music track to %i!", V_atoi(args.Arg(1)));
	}
}

CON_COMMAND(p2mm_gelocity_start, "Starts the Gelocity race.")
{
	// Check if host is in a gelocity map.
	if (!InGelocityMap())
	{
		Log(WARNING, false, "Not currently in a Gelocity map!");
		return;
	}

	// Check if the gelocity race is already going. Make sure to not mess with the race's laps.
	ScriptVariant_t raceStartedScript;
	g_pScriptVM->GetValue("b_RaceStarted", &raceStartedScript);
	if (raceStartedScript.m_bool)
	{
		Log(WARNING, false, "Race is currently in progress!");
		return;
	}

	g_pScriptVM->Run("StartGelocityRace();", false);
}

//---------------------------------------------------------------------------------
// P2:MM Improved kick and ban ConCommands
//---------------------------------------------------------------------------------
std::vector<RemovePlayerInfo> banList;

// Have to make a ConCommand to remove the player becase 
void RemovePlayerOperation(const bool bBanning, const int userid)
{
	RemovePlayerInfo bannedPlayer;
	bannedPlayer.userID = userid;
	player_info_t playerInfo;
	engineServer->GetPlayerInfo(UserIDToPlayerIndex(bannedPlayer.userID), &playerInfo);
	bannedPlayer.username = std::string(playerInfo.name);
	bannedPlayer.guid = std::string(playerInfo.guid);

	// The first argument determines if the player will be banned or kicked. True is ban.
	if (bBanning)
	{
		for (auto& i : banList)
		{
			if (FStrEq(i.username.c_str(), bannedPlayer.username.c_str()))
			{
				Log(WARNING, false, "Ban called on player that is already banned!");
				UTIL_ClientPrint(UTIL_PlayerByIndex(0), HUD_PRINTTALK, "\x03(P2:MM): This player is already banned!");
				return;
			}
		}

		// Use disconnect to ban the player because normal kick isn't consistent and so we have a custom disconnect message.
		std::string bannedStr = std::string(_bstr_t(g_pLocalize->FindSafe("#P2MM_BannedFromServer")));
		std::string opCMD = std::string("disconnect \"" + bannedStr + "\"");
		std::string banMsg = std::string("\x03(P2:MM): Player " + bannedPlayer.username + " has been banned!");
		UTIL_ClientPrint(UTIL_PlayerByIndex(0), HUD_PRINTTALK, banMsg.c_str());
		//CGameClient__ExecuteStringCommand(CBaseServer__GetClient(UserIDToPlayerIndex(bannedPlayer.userID)), opCMD.c_str());
		banList.push_back(bannedPlayer);
	}
	else
	{
		// Use disconnect to kick the player because normal kick isn't consistent and so we have a custom disconnect message.
		std::string kickedStr = std::string(_bstr_t(g_pLocalize->FindSafe("#P2MM_KickedFromServer")));
		std::string opCMD = std::string("disconnect \"" + kickedStr + "\"");
		std::string kickMsg = std::string("\x03(P2:MM): Player " + bannedPlayer.username + " has been kicked!");
		UTIL_ClientPrint(UTIL_PlayerByIndex(0), HUD_PRINTTALK, kickMsg.c_str());
		//CGameClient__ExecuteStringCommand(CBaseServer__GetClient(UserIDToPlayerIndex(bannedPlayer.userID)), opCMD.c_str());
	}
	engineClient->ExecuteClientCmd("gameui_hide");

	Log(INFO, true, "Banning?: %i", bBanning);
	Log(INFO, true, "userID: %i", bannedPlayer.userID);
	Log(INFO, true, "username: %s", bannedPlayer.username.c_str());
	Log(INFO, true, "guid: %s", bannedPlayer.guid.c_str());
}

// Display UI for either banning or kicking so host can ban or kick a player.
void RemovePlayerUI(const int playerIndex, const bool bBanning)
{
	if (!IsGameActive())
	{
		Log(WARNING, false, "Game session is not currently running!");
		return;
	}

	engineClient->ExecuteClientCmd("gameui_activate"); // Doesn't work for some reason although it does for the first run prompt.
	//CGameClient__ExecuteStringCommand(CBaseServer__GetClient(playerIndex), "gameui_activate");
	std::vector<RemovePlayerInfo> userList;
	FOR_ALL_PLAYERS(i)
	{
		if (i == 1) continue; // Don't add host to button options.
		player_info_t playerInfo;
		engineServer->GetPlayerInfo(i, &playerInfo);
		RemovePlayerInfo curUserInfo;
		curUserInfo.userID = playerInfo.userID;
		curUserInfo.username = playerInfo.name;
		curUserInfo.guid = playerInfo.guid;
		userList.push_back(curUserInfo);
	}

	KeyValues* menuData = new KeyValues("removeplayermenu");
	menuData->SetWString("title", bBanning ? g_pLocalize->FindSafe("P2MM_BanMenu_t") : g_pLocalize->FindSafe("P2MM_KickMenu_t"));
	menuData->SetWString("msg", bBanning ? g_pLocalize->FindSafe("P2MM_BanMenu_d") : g_pLocalize->FindSafe("P2MM_KickMenu_d"));
	menuData->SetInt("level", 0);
	menuData->SetInt("time", 20);

	char num[32], msg[MAX_PLAYER_NAME_LENGTH], cmd[512];
	for (size_t i = 0; i < userList.size(); i++)
	{
		V_snprintf(num, sizeof(num), "%i", i);
		V_snprintf(msg, sizeof(msg), "%s", userList[i].username.c_str());
		V_snprintf(cmd, sizeof(cmd), "removeplayeroperation %i %i", bBanning, userList[i].userID);

		KeyValues* item = menuData->FindKey(num, &g_P2MMServerPlugin);
		item->SetString("msg", msg);
		item->SetString("command", cmd);
	}
	
	g_pPluginHelpers->CreateMessage(INDEXENT(playerIndex), DIALOG_MENU, menuData, &g_P2MMServerPlugin);
	menuData->deleteThis();
}

CON_COMMAND(p2mm_kick, "Kick a player from the P2:MM play session.")
{
	
	RemovePlayerUI(UTIL_GetCommandClientIndex() + 1, false);
}

CON_COMMAND(ban, "Ban a player from the P2:MM play session.")
{
	RemovePlayerUI(UTIL_GetCommandClientIndex() + 1, true);
}

CON_COMMAND(unban, "Unban a player from the P2:MM play session.")
{
	if (!IsGameActive())
	{
		Log(WARNING, false, "Game session is not currently running!");
		return;
	}

	engineClient->ExecuteClientCmd("gameui_activate"); // Doesn't work for some reason although it does for the first run prompt.
	//CGameClient__ExecuteStringCommand(CBaseServer__GetClient(playerIndex), "gameui_activate");
	KeyValues* menuData = new KeyValues("unbanmenu");
	menuData->SetWString("title", g_pLocalize->FindSafe("P2MM_BanMenu_t"));
	menuData->SetWString("msg", g_pLocalize->FindSafe("P2MM_BanMenu_d"));
	menuData->SetInt("level", 0);
	menuData->SetInt("time", 20);

	char num[32], msg[MAX_PLAYER_NAME_LENGTH], cmd[512];
	for (size_t i = 0; i < banList.size(); i++)
	{
		V_snprintf(num, sizeof(num), "%i", i);
		V_snprintf(msg, sizeof(msg), "%s", banList[i].username.c_str());
		V_snprintf(cmd, sizeof(cmd), "unbanoperation %i", banList[i].userID); // Add unbanoperation to clientcommand callback

		KeyValues* item = menuData->FindKey(num, &g_P2MMServerPlugin);
		item->SetString("msg", msg);
		item->SetString("command", cmd);
	}

	g_pPluginHelpers->CreateMessage(INDEXENT(UTIL_GetCommandClientIndex() + 1), DIALOG_MENU, menuData, &g_P2MMServerPlugin);
	menuData->deleteThis();
}