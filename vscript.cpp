//===========================================================================//
//
// Author: Nanoman2525 & Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#include "sdk.hpp"
#include "commands.hpp"
#include "p2mm.hpp"

//---------------------------------------------------------------------------------
// Purpose: Logging for the P2MM VScript. The log message must be passed as a string or it will error.
//---------------------------------------------------------------------------------
static void printlP2MM(const int level, const bool dev, const char* pMsgFormat)
{
	if (dev && !p2mm_developer.GetBool())
		return;

	va_list argPtr;
	char szFormattedText[1024] = { 0 };
	va_start(argPtr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argPtr);
	va_end(argPtr);

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
// Purpose: Returns true if this is a dedicated server.
//---------------------------------------------------------------------------------
static bool IsDedicatedServer()
{
	return engineServer->IsDedicatedServer();
}

//---------------------------------------------------------------------------------
// Purpose: Initializes, spawns, then activates an entity in the map.
// Create a entity using CreateByClassname in VScript, then use this function on its handle.
// Note: Not all entities will work even after being initialized with this function.
//---------------------------------------------------------------------------------
static void InitializeEntity(const HSCRIPT ent)
{
	// Get the entity of the instance.
	const auto pEntity = static_cast<CBaseEntity*>(HSCRIPTENT(ent));
	if (!pEntity)
		return;

	// Spawn the new entity into the world.
	g_pServerTools->DispatchSpawn(pEntity);
	// Call the entities Activate class function to make it fully work.
	static auto Activate = *reinterpret_cast<void(__thiscall**)(void*)>(*reinterpret_cast<uintptr_t*>(pEntity) + 148);
	Activate(pEntity);
}

//---------------------------------------------------------------------------------
// Purpose: Sends a raw message to the chat HUD. Specifying no playerIndex or 0 sends to all players.
//			Supports printing localization strings but those that require formatting can't be formatted.
//---------------------------------------------------------------------------------
static void SendToChat(const int playerIndex, const char* msg)
{
	if (!msg)
		return;

	if (!playerIndex)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerInfo;
			if (engineServer->GetPlayerInfo(i, &playerInfo))
			{
				if (CBasePlayer* pPlayer = UTIL_PlayerByIndex(i)) 
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
		Log(WARNING, false, "Invalid player index specified for SendToChat! playerIndex: \"%i\"", playerIndex);
		return;
	}
	UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, msg);
}

//---------------------------------------------------------------------------------
// Purpose: Shows the first run prompt if enabled in config.nut.
//---------------------------------------------------------------------------------
static void CallFirstRunPrompt()
{
	// Don't display again once the first one is shown.
	if (g_P2MMServerPlugin.m_bSeenFirstRunPrompt)
	{
		Log(INFO, true, "First run prompt already shown...");
		return;
	}

	Log(INFO, true, "DISPLAYING FIRST RUN PROMPT!");

	// Put together KeyValues to pass to CreateMessage.
	KeyValues* kv = new KeyValues("firstrunprompt");
	kv->SetInt("level", 1);
	kv->SetWString("title", g_pLocalize->FindSafe("#P2MM_FirstRunPrompt_t"));
	kv->SetWString("msg", g_pLocalize->FindSafe("#P2MM_FirstRunPrompt_d"));

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
static void ConsolePrint(const int playerIndex, const char* msg)
{
	if (!msg) return;

	if (!playerIndex)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerInfo;
			if (engineServer->GetPlayerInfo(i, &playerInfo))
			{
				if (CBasePlayer* pPlayer = UTIL_PlayerByIndex(i))
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
		Log(WARNING, false, "Invalid player index passed into ConsolePrint! playerIndex: \"%i\"", playerIndex);
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
static void ClientPrint(const int playerIndex, const char* msg)
{
	if (!msg) return;

	if (!playerIndex)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerInfo;
			if (engineServer->GetPlayerInfo(i, &playerInfo))
			{
				if (CBasePlayer* pPlayer = UTIL_PlayerByIndex(i))
					UTIL_ClientPrint(pPlayer, HUD_PRINTCENTER, msg);
			}
		}
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		Log(WARNING, false, "Invalid player index specified for ClientPrint! playerIndex: \"%i\"", playerIndex);
		return;
	}

	UTIL_ClientPrint(pPlayer, HUD_PRINTCENTER, msg);
}

/**
 * @brief Print a message to the screen based on what the game_text entity does. See the Valve Developer Commentary page for the game_text entity to read
 *		 what each field does and what values to input. Specifying no playerIndex or 0 sends to all players.
 *		 Supports printing localization strings but those that require formatting can't be formatted.
 *		 Vectors are used to consolidate some parameters so function isn't monstrously big.
 * @param playerIndex Player index to display message to, or 0 to send to all players
 * @param msg Message that should be displayed.
 * @param posChannel Location on screen and channel to display message to.
 * @param effect Effect text should use when displaying.
 * @param fxTime How long the effect should last.
 * @param RGB1 Color of text.
 * @param alpha1 Alpha of text.
 * @param RGB2 Transition color of text.
 * @param alpha2 Transition color of text.
 * @param showTimes Fade in, fade out, and hold time of text.
 */
static void HudPrint
	(
	const int playerIndex, const char* msg,
	const Vector& posChannel, const int effect, const float fxTime,
	const Vector& RGB1, const int alpha1, const Vector& RGB2, const int alpha2,
	const Vector& showTimes
	)
{
	if (!msg)
		return;

	HudMessageParams hudTextParams;
	hudTextParams.x = posChannel.x;
	hudTextParams.y = posChannel.y;
	hudTextParams.channel = static_cast<int>(posChannel.z);
	hudTextParams.effect = effect;
	hudTextParams.fxTime = fxTime;
	hudTextParams.r1 = static_cast<byte>(RGB1.x);
	hudTextParams.g1 = static_cast<byte>(RGB1.y);
	hudTextParams.b1 = static_cast<byte>(RGB1.z);
	hudTextParams.a1 = static_cast<byte>(alpha1);
	hudTextParams.r2 = static_cast<byte>(RGB2.x);
	hudTextParams.g2 = static_cast<byte>(RGB2.y);
	hudTextParams.b2 = static_cast<byte>(RGB2.z);
	hudTextParams.a2 = static_cast<byte>(alpha2);
	hudTextParams.fadeinTime = showTimes.x;
	hudTextParams.fadeoutTime = showTimes.y;
	hudTextParams.holdTime = showTimes.z;

	if (!playerIndex)
	{
		UTIL_HudMessage(nullptr, hudTextParams, msg);
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		Log(WARNING, false, "Invalid playerIndex passed into HudPrint! playerIndex: \"%i\"", playerIndex);
		return;
	}

	UTIL_HudMessage(pPlayer, hudTextParams, msg);
}

/**
 * @brief Returns the current max players in the server.
 * @return Max players in server.
 */
static int GetMaxPlayers()
{
	return MAX_PLAYERS;
}

/**
 * @brief Enable or disable displaying the score board for a player.
 * @param playerIndex Index of player to enable/disable the scoreboard of.
 * @param enable Whather the scoreboard should be enabled for this player.
 */
static void ShowScoreboard(const int playerIndex, const bool enable)
{
	CBasePlayer__ShowViewPortPanel(playerIndex, "scores", enable);
}

/**
 * @brief Improved version of TraceLine for VScript that allows for checking with bit masks and collision groups.
 * @param vecAbsStart Location to start trace line.
 * @param vecAbsEnd Location where the trace line should end.
 * @param mask Bit mask used for determining what objects should be hit.
 * @param ignore A entity for the trace line to ignore, typically this is for a trace line originating from a entity.
 * @param collisionGroup Collision group that the trace line can hit.
 * @return Returns a fraction of line where a collision occured, or if in a solid, the amount spent outside the solid.
 */
static float Script_UTIL_TraceLine(const Vector& vecAbsStart, const Vector& vecAbsEnd, const int mask, const HSCRIPT ignore, const int collisionGroup)
{
	trace_t trace;
	UTIL_TraceLine(vecAbsStart, vecAbsEnd, mask, static_cast<const IHandleEntity*>(HSCRIPTENT(ignore)), collisionGroup, &trace);
	
	if (trace.fractionleftsolid >= 1.0 && trace.startsolid)
		return 1.0f - trace.fractionleftsolid;
		
	return trace.fraction;
}

/**
 * @brief Get the eye angles of a player.
 * @param playerIndex Player index to get angles of.
 * @return Vector of player eye angles.
 */
static Vector Script_EyeAngles(const int playerIndex)
{
	const QAngle eyeAngles = CBasePlayer__EyeAngles(playerIndex);
	return Vector(eyeAngles.x, eyeAngles.y, eyeAngles.z);
}

void RegisterFuncsAndRun()
{
	// The IScriptVM interface has to be retrieved later than when starting the plugin as it isn't available yet in memory until map is loading.
	Log(INFO, true, "Loading g_pScriptVM...");
	g_pScriptVM = **Memory::Scanner::Scan<IScriptVM***>(SERVERDLL, "8B 1D ?? ?? ?? ?? 57 85 DB", 2);
	if (!g_pScriptVM)
	{
		assert(0 && "Unable to load g_pScriptVM!");
		Log(ERRORR, false, "P2:MM was unable to load g_pScriptVM!\nThis is required for P2:MM to run!\nPlease report!");
		return;
	}

	ScriptRegisterFunction	   (g_pScriptVM, printlP2MM, "Logging for the P2MM VScript. The log message must be passed as a string or it will error.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetPlayerName, "GetPlayerName", "Gets player username by their entity index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetSteamID, "GetSteamID", "Gets the account ID component of player SteamID by the player's entity index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, UserIDToPlayerIndex, "UserIDToPlayerIndex", "Get the player's entity index by their userid.");
	ScriptRegisterFunction	   (g_pScriptVM, IsMapValid, "Returns true is the supplied string is a available map to load and run.");
	ScriptRegisterFunction	   (g_pScriptVM, GetDeveloperLevelP2MM, "Returns the value of ConVar p2mm_developer.");
	ScriptRegisterFunction	   (g_pScriptVM, IsDedicatedServer, "Returns true if this is a dedicated server.");
	ScriptRegisterFunction	   (g_pScriptVM, InitializeEntity, "Initializes an entity. Note: Not all entities will work even after being initialized with this function.");
	ScriptRegisterFunction	   (g_pScriptVM, SendToChat, "Sends a raw message to the chat HUD. Specifying no playerIndex or 0 sends to all players. Supports printing localization strings but those that require formatting can't be formatted.");
	ScriptRegisterFunction	   (g_pScriptVM, GetGameMainDir, "Returns the current game directory. Ex. portal2");
	ScriptRegisterFunction	   (g_pScriptVM, GetGameRootDir, "Returns the current root game directory. Ex. Portal 2");
	ScriptRegisterFunction	   (g_pScriptVM, CallFirstRunPrompt, "Shows the first run prompt if enabled in config.nut.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetConVarInt, "GetConVarInt", "Get the integer value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GetConVarString, "GetConVarString", "Get the string value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, SetConVarInt, "SetConVarInt", "Set the integer value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, SetConVarString, "SetConVarString", "Set the string value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, INDEXHANDLE, "PlayerByIndex", "Takes the player's entity index and returns the player's script handle.");
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
													   "what each field does and what values to input. Specifying no playerIndex or 0 sends to all players."
													   "Supports printing localization strings but those that require formatting can't be formatted."
													   "Vectors are used to consolidate some parameters so function isn't monstrously big."
	);
	ScriptRegisterFunction	   (g_pScriptVM, GetMaxPlayers, "Return max amount of players that can be in the server.");
	ScriptRegisterFunction	   (g_pScriptVM, ShowScoreboard, "Enable or disable displaying the score board for players.");
	ScriptRegisterFunction	   (g_pScriptVM, RemovePlayerUI, "Display UI for either banning or kicking so host can ban or kick a player.");
	ScriptRegisterFunctionNamed(g_pScriptVM, Script_UTIL_TraceLine, "TraceLineEx", "Improved version of TraceLine that allows for checking with bit masks and collision groups.");
	ScriptRegisterFunctionNamed(g_pScriptVM, Script_EyeAngles, "EyeAngles", "Get the eye angles of a player.");

	// Load up the main P2:MM VScript.
	g_pScriptVM->Run("IncludeScript(\"multiplayermod/p2mm\");");
}
