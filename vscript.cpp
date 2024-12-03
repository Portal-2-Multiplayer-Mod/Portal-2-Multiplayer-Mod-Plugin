//===========================================================================//
//
// Author: Nanoman2525 & Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#include "p2mm.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar p2mm_developer;
extern ConVar p2mm_lastmap;

//---------------------------------------------------------------------------------
// Purpose: Logging for the P2MM VScript. The log message must be passed as a string or it will error.
//---------------------------------------------------------------------------------
static void printlP2MM(int level, bool dev, const char* pMsgFormat)
{
	if (dev && !p2mm_developer.GetBool()) return;

	va_list argptr;
	char szFormattedText[1024] = { 0 };
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	char completeMsg[1024];
	V_snprintf(completeMsg, sizeof(completeMsg), "(P2:MM VSCRIPT): %s\n", szFormattedText);

	switch (level)
	{
		case 0:
			ConColorMsg(P2MM_VSCRIPT_CONSOLE_COLOR, completeMsg);
			return;
		case 1:
			Warning(completeMsg);
			return;
		default:
			Warning("(P2:MM VSCRIPT): printlP2MM level set outside of 0-1, \"%i\", defaulting to ConColorMsg().\n", level);
			ConColorMsg(P2MM_VSCRIPT_CONSOLE_COLOR, completeMsg);
			return;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Returns true is the supplied string is a available map to load and run.
//---------------------------------------------------------------------------------
static bool IsMapValid(const char* map)
{
	return engineServer->IsMapValid(map);
}

//---------------------------------------------------------------------------------
// Purpose: Returns the value of ConVar p2mm_developer.
//---------------------------------------------------------------------------------
static int GetDeveloperLevelP2MM()
{
	return p2mm_developer.GetInt();
}

//---------------------------------------------------------------------------------
// Purpose: Sets 'player_held_object_use_view_model' to the supplied integer value.
//---------------------------------------------------------------------------------
static void SetPhysTypeConVar(int newval)
{
	g_pCVar->FindVar("player_held_object_use_view_model")->SetValue(newval);
}

//---------------------------------------------------------------------------------
// Purpose: Sets 'portal_max_separation_force' to the supplied integer value.
//---------------------------------------------------------------------------------
static void SetMaxPortalSeparationConvar(int newval)
{
	if (engineServer->IsDedicatedServer())
	{
		P2MMLog(1, true, "SetMaxPortalSeparationConVar can not be set on dedicated servers!");
		return;
	}
	g_pCVar->FindVar("portal_max_separation_force")->SetValue(newval);
}

//---------------------------------------------------------------------------------
// Purpose: Returns true if this is a dedicated server.
//---------------------------------------------------------------------------------
static bool IsDedicatedServer()
{
	return engineServer->IsDedicatedServer();
}

//---------------------------------------------------------------------------------
// Purpose: Initializes, spawns, then activates an entity in the map.
//---------------------------------------------------------------------------------
static void InitializeEntity(HSCRIPT ent)
{
	static uintptr_t func = (uintptr_t)Memory::Scanner::Scan<void*>(SERVERDLL, "E8 ?? ?? ?? ?? 8B 4D 18 8B 57 5C", 1);
	static auto GetCBaseEntityScriptDesc = reinterpret_cast<ScriptClassDesc_t * (*)()>(*reinterpret_cast<uintptr_t*>(func) + func + sizeof(func));
	void* pEntity = reinterpret_cast<void*>(g_pScriptVM->GetInstanceValue(ent, GetCBaseEntityScriptDesc()));;
	if (pEntity)
	{
		g_pServerTools->DispatchSpawn(pEntity);

		static auto Activate = *reinterpret_cast<void(__thiscall**)(void*)>(*reinterpret_cast<uintptr_t*>(pEntity) + 148);
		Activate(pEntity);
	}
}

//---------------------------------------------------------------------------------
// Purpose: Sends a raw message to the chat HUD. Specifying no playerIndex or 0 sends to all players.
//			Supports printing localization strings but those that require formatting can't be formatted.
//---------------------------------------------------------------------------------
static void SendToChat(const char* msg, int playerIndex)
{
	if (!msg) return;

	if (!playerIndex)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerinfo;
			if (engineServer->GetPlayerInfo(i, &playerinfo))
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				if (pPlayer) 
				{
					UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, msg);
				}
			}
		}
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, false, "Invalid player index specified for SendToChat! playerIndex: \"%i\"", playerIndex);
		return;
	}
	UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, msg);
}

//---------------------------------------------------------------------------------
// Purpose: Returns the last map recorded by the launcher's Last Map System.
//---------------------------------------------------------------------------------
static const char* GetLastMap()
{
	return p2mm_lastmap.GetString();
}

//---------------------------------------------------------------------------------
// Purpose: Get or set the state of whether the first map was run or not.
// Set false/true = 0/1 | -1 to get state.
//---------------------------------------------------------------------------------
static bool FirstRunState(int state)
{
	if (state == 0 || state == 1)
		return g_P2MMServerPlugin.m_bFirstMapRan = !!state;
	
	return g_P2MMServerPlugin.m_bFirstMapRan;
}

//---------------------------------------------------------------------------------
// Purpose: Shows the first run prompt if enabled in config.nut.
//---------------------------------------------------------------------------------
static void CallFirstRunPrompt()
{
	// Don't display again once the first one is shown.
	if (g_P2MMServerPlugin.m_bSeenFirstRunPrompt)
	{
		P2MMLog(0, true, "First run prompt already shown...");
		return;
	}

	P2MMLog(0, true, "DISPLAYING FIRST RUN PROMPT!");

	// Put together KeyValues to pass to CreateMessage.
	KeyValues* kv = new KeyValues("firstrunprompt");
	kv->SetInt("level", 0);
	kv->SetWString("title", g_pLocalize->FindSafe("#P2MM_FirstRunPrompt_t"));
	//kv->SetString("title", "Welcome to the Portal 2: Multiplayer Mod!");
	kv->SetWString("msg", g_pLocalize->FindSafe("#P2MM_FirstRunPrompt_d"));
	/*kv->SetString("msg",
		"Welcome to the Portal 2: Multiplayer Mod!\n\n"
		"Input '!help' into chat to see a full list of chat commands you can use!\n"
		"Hope you enjoy the mod! - Portal 2: Multiplayer Mod Team\n\n"
		"This message can be disabled in config.nut located in the local p2mm folder on your system.\n"
		"'p2mm/ModFiles/Portal 2/install_dlc/scripts/vscripts/multiplayermod/config.nut'"
	);*/

	// CreateMessage prompts can only be seen when the pause menu is up, so pause the game.
	engineClient->ExecuteClientCmd("gameui_activate");
	g_pPluginHelpers->CreateMessage(INDEXENT(1), DIALOG_TEXT, kv, &g_P2MMServerPlugin);
	kv->deleteThis();

	// Set the plugin variable flag that the host seen the prompt to true so its not reshown.
	g_P2MMServerPlugin.m_bSeenFirstRunPrompt = true;
}

//---------------------------------------------------------------------------------
// Purpose: Print a message to a player's console, unlike printl() which is just the host.
//			Specifying no playerIndex or 0 sends to all players.
//			Supports printing localization strings but those that require formatting can't be formatted.
//---------------------------------------------------------------------------------
void ConsolePrint(int playerIndex, const char* msg)
{
	if (!msg) return;

	if (!playerIndex)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerinfo;
			if (engineServer->GetPlayerInfo(i, &playerinfo))
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				if (pPlayer)
				{
					UTIL_ClientPrint(pPlayer, HUD_PRINTCONSOLE, msg);
				}
			}
		}
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, false, "Invalid player index passed into ConsolePrint! playerIndex: \"%i\"", playerIndex);
		return;
	}

	std::string fixedMsg = std::string(msg) + "\n";
	UTIL_ClientPrint(pPlayer, HUD_PRINTCONSOLE, fixedMsg.c_str());
}

//---------------------------------------------------------------------------------
// Purpose: Print a message to the top center position of a player's screen.
//			Specifying no playerIndex or 0 sends to all players.
//			Supports printing localization strings but those that require formatting can't be formatted.
//---------------------------------------------------------------------------------
void ClientPrint(int playerIndex, const char* msg)
{
	if (!msg) return;

	if (!playerIndex)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerinfo;
			if (engineServer->GetPlayerInfo(i, &playerinfo))
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				if (pPlayer)
				{
					UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, msg);
				}
			}
		}
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, false, "Invalid player index specified for ClientPrint! playerIndex: \"%i\"", playerIndex);
		return;
	}

	UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, msg);
}

//---------------------------------------------------------------------------------
// Purpose: Print a message to the screen based on what the game_text entity does.
//			See the Valve Developer Commentary page for the game_text entity to read
//			what each field does. Specifying no playerIndex or 0 sends to all players.
//			Supports printing localization strings but those that require formatting can't be formatted.
//			Vectors are in place for sets of RGB values.
//			Vector is used to consolidate x, y, and channel parameters together.
//			Vector is used to consolidate fadeinTime, fadeoutTime, and holdTime.
//---------------------------------------------------------------------------------
void HudPrint
	(
	int playerIndex, const char* msg, 
	Vector posChannel, int effect, float fxTime,
	Vector RGB1, int alpha1, Vector RGB2, int alpha2,
	Vector showTimes
	)
{
	if (!msg) return;

	hudtextparms_t hudTextParams;
	hudTextParams.x = posChannel.x;
	hudTextParams.y = posChannel.y;
	hudTextParams.channel = posChannel.z;
	hudTextParams.effect = effect;
	hudTextParams.fxTime = fxTime;
	hudTextParams.r1 = RGB1.x;
	hudTextParams.g1 = RGB1.y;
	hudTextParams.b1 = RGB1.z;
	hudTextParams.a1 = alpha1;
	hudTextParams.r2 = RGB2.x;
	hudTextParams.g2 = RGB2.y;
	hudTextParams.b2 = RGB2.z;
	hudTextParams.a2 = alpha2;
	hudTextParams.fadeinTime = showTimes.x;
	hudTextParams.fadeoutTime = showTimes.y;
	hudTextParams.holdTime = showTimes.z;

	if (!playerIndex)
	{
		UTIL_HudMessage(NULL, hudTextParams, msg);
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, false, "Invalid playerIndex passed into HudPrint! playerIndex: \"%i\"", playerIndex);
		return;
	}

	UTIL_HudMessage(pPlayer, hudTextParams, msg);
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
int GetMaxPlayers()
{
	return MAX_PLAYERS;
}

//---------------------------------------------------------------------------------
// Purpose: Enable or disable displaying the score board for a player.
//---------------------------------------------------------------------------------
void ShowScoreboard(int playerIndex, bool bEnable)
{
	CBasePlayer__ShowViewPortPanel(playerIndex, "scores", bEnable);
}
	

void RegisterFuncsAndRun()
{
	g_pScriptVM = **Memory::Scanner::Scan<IScriptVM***>(SERVERDLL, "8B 1D ?? ?? ?? ?? 57 85 DB", 2);
	if (!g_pScriptVM)
	{
		P2MMLog(1, false, "Could not register or run our VScript functions!");
		return;
	}

	ScriptRegisterFunction	   (g_pScriptVM, printlP2MM, "Logging for the P2MM VScript. The log message must be passed as a string or it will error.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetPlayerName, "GetPlayerName", "Gets player username by their entity index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetSteamID, "GetSteamID", "Gets the account ID component of player SteamID by the player's entity index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, UserIDToPlayerIndex, "UserIDToPlayerIndex", "Get the player's entity index by their userid.");
	ScriptRegisterFunction	   (g_pScriptVM, IsMapValid, "Returns true is the supplied string is a available map to load and run.");
	ScriptRegisterFunction	   (g_pScriptVM, GetDeveloperLevelP2MM, "Returns the value of ConVar p2mm_developer.");
	ScriptRegisterFunction	   (g_pScriptVM, SetPhysTypeConVar, "Sets 'player_held_object_use_view_model' to the supplied integer value.");
	ScriptRegisterFunction	   (g_pScriptVM, SetMaxPortalSeparationConvar, "Sets 'portal_max_separation_force' to the supplied integer value.");
	ScriptRegisterFunction	   (g_pScriptVM, IsDedicatedServer, "Returns true if this is a dedicated server.");
	ScriptRegisterFunction	   (g_pScriptVM, InitializeEntity, "Initializes an entity. Note: Not all entities will work even after being initialized with this function.");
	ScriptRegisterFunction	   (g_pScriptVM, SendToChat, "Sends a raw message to the chat HUD. Specifying no playerIndex or 0 sends to all players. Supports printing localization strings but those that require formatting can't be formatted.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetGameMainDir, "GetGameMainDir", "Returns the game directory. Ex. portal2");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetGameBaseDir, "GetGameBaseDir", "Get the main game directory being used. Ex. Portal 2");
	ScriptRegisterFunction	   (g_pScriptVM, GetLastMap, "Returns the last map recorded by the launcher's Last Map system.");
	ScriptRegisterFunction	   (g_pScriptVM, FirstRunState, "Get or set the state of whether the first map was run or not. Set false/true = 0/1 | -1 to get state.");
	ScriptRegisterFunction	   (g_pScriptVM, CallFirstRunPrompt, "Shows the first run prompt if enabled in config.nut.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetConVarInt, "GetConVarInt", "Get the integer value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetConVarString, "GetConVarString", "Get the string value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, SetConVarInt, "SetConVarInt", "Set the integer value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, SetConVarString, "SetConVarString", "Set the string value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, INDEXHANDLE, "UTIL_PlayerByIndex", "Takes the player's entity index and returns the player's script handle.");
	ScriptRegisterFunctionNamed(g_pScriptVM, CPortal_Player__RespawnPlayer, "RespawnPlayer", "Respawn the a player by their entity index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, CPortal_Player__SetFlashlightState, "SetFlashlightState", "Set the flashlight for a player on or off.");
	ScriptRegisterFunction     (g_pScriptVM, ConsolePrint, "Print a message to the top center position of a player's screen. Specifying no playerIndex or 0 sends to all players."
														   "Supports printing localization strings but those that require formatting can't be formatted."
	);
	ScriptRegisterFunction     (g_pScriptVM, ClientPrint, "Print a message to the top center position of a player's screen. Specifying no playerIndex or 0 sends to all players."
														  "Supports printing localization strings but those that require formatting can't be formatted."
	);
	ScriptRegisterFunction     (g_pScriptVM, HudPrint, "Print a message to the screen based on what the game_text entity does."
													   "See the Valve Developer Commentary page for the game_text entity to read"
													   "what each field does. Specifying no playerIndex or 0 sends to all players."
													   "Supports printing localization strings but those that require formatting can't be formatted."
													   "Vectors are in place for sets of RGB values."
													   "Vector is used to consolidate x, y, and channel parameters together."
													   "Vector is used to consolidate fadeinTime, fadeoutTime, and holdTime."
	);
	ScriptRegisterFunction		(g_pScriptVM, GetMaxPlayers, "Self-explanatory.");
	ScriptRegisterFunction		(g_pScriptVM, ShowScoreboard, "Enable or disable displaying the score board for players.");

	// Load up the main P2:MM VScript and set
	g_pScriptVM->Run("IncludeScript(\"multiplayermod/p2mm\");");
}
