//===========================================================================//
//
// Author: Nanoman2525
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#include "p2mm.hpp"

#include "irecipientfilter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar p2mm_developer;
extern ConVar p2mm_lastmap;

//---------------------------------------------------------------------------------
// Purpose: Logging for the P2MM VScript. The log message must be passed as a string or it will error.
//---------------------------------------------------------------------------------
static void printlP2MM(int level, bool dev, const char* pMsgFormat)
{
	va_list argptr;
	char szFormattedText[1024];
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	char completeMsg[1024];
	V_snprintf(completeMsg, sizeof(completeMsg), "(P2:MM VSCRIPT): %s\n", szFormattedText);

	if (dev && !p2mm_developer.GetBool())
	{
		return;
	}

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
// Purpose: Initializes an entity.
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

class CFilter : public IRecipientFilter
{
public:
	CFilter() { recipient_count = 0; };
	~CFilter() {};

	virtual bool IsReliable() const { return false; };
	virtual bool IsInitMessage() const { return false; };

	virtual int GetRecipientCount() const { return recipient_count; };
	virtual int GetRecipientIndex(int slot) const { return ((slot < 0) || (slot >= recipient_count)) ? -1 : recipients[slot]; };
	void AddPlayer(int player_index)
	{
		if (recipient_count > 255)
		{
			return;
		}

		recipients[recipient_count] = player_index;
		recipient_count++;
	}

private:
	int recipients[256];
	int recipient_count;
};

//---------------------------------------------------------------------------------
// Purpose: Sends a raw message to the chat HUD.
//---------------------------------------------------------------------------------
static void SendToChat(const char* msg, int playerIndex)
{
	if (!msg)
	{
		return;
	}
	bf_write* netmsg;
	CFilter recipient_filter;

	// Send to all players
	if (playerIndex == 0)
	{
		for (int i = 1; i < g_pGlobals->maxClients; i++)
		{
			player_info_t playerinfo;
			if (engineServer->GetPlayerInfo(i, &playerinfo))
			{
				recipient_filter.AddPlayer(i);
			}
		}
	}
	else
	{
		recipient_filter.AddPlayer(playerIndex);
	}

	netmsg = engineServer->UserMessageBegin(&recipient_filter, 4, "SayText2");
	netmsg->WriteByte(0);
	netmsg->WriteString(msg);
	netmsg->WriteByte(1);
	engineServer->MessageEnd();
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
	if (state == 0 || state == 1) {
		return g_P2MMServerPlugin.m_bFirstMapRan = !!state;
	}
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

	P2MMLog(0, false, "DISPLAYING FIRST RUN PROMPT!");

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
	pluginHelpers->CreateMessage(INDEXENT(1), DIALOG_TEXT, kv, &g_P2MMServerPlugin);
	kv->deleteThis();
	
	// Set the plugin variable flag that the host seen the prompt to true so its not reshown.
	g_P2MMServerPlugin.m_bSeenFirstRunPrompt = true;
}

void RegisterFuncsAndRun()
{
	g_pScriptVM = **Memory::Scanner::Scan<IScriptVM***>(SERVERDLL, "8B 1D ?? ?? ?? ?? 57 85 DB", 2); // crashes infra, bytes incorrect
	if (!g_pScriptVM)
	{
		P2MMLog(1, false, "Could not run or register our VScript functions!");
		return;
	}

	ScriptRegisterFunction	   (g_pScriptVM, printlP2MM, "Logging for the P2MM VScript. The log message must be passed as a string or it will error.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::GetPlayerName, "GetPlayerName", "Gets player username by index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::GetSteamID, "GetSteamID", "Gets the account ID component of player SteamID by index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::UserIDToPlayerIndex, "UserIDToPlayerIndex", "Gets player entity index by userid.");
	ScriptRegisterFunction	   (g_pScriptVM, IsMapValid, "Returns true is the supplied string is a available map to load and run.");
	ScriptRegisterFunction	   (g_pScriptVM, GetDeveloperLevelP2MM, "Returns the value of ConVar p2mm_developer.");
	ScriptRegisterFunction	   (g_pScriptVM, SetPhysTypeConVar, "Sets 'player_held_object_use_view_model' to the supplied integer value.");
	ScriptRegisterFunction	   (g_pScriptVM, SetMaxPortalSeparationConvar, "Sets 'portal_max_separation_force' to the supplied integer value.");
	ScriptRegisterFunction	   (g_pScriptVM, IsDedicatedServer, "Returns true if this is a dedicated server.");
	ScriptRegisterFunction	   (g_pScriptVM, InitializeEntity, "Initializes an entity. Note: Not all entities will work even after being initialized with this function.");
	ScriptRegisterFunction	   (g_pScriptVM, SendToChat, "Sends a raw message to the chat HUD.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::GetGameMainDir, "GetGameMainDir", "Returns the game directory. Ex. portal2/portal_stories");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::GetGameBaseDir, "GetGameBaseDir", "Get the main game directory being used. Ex. Portal 2/Portal Stories Mel");
	ScriptRegisterFunction	   (g_pScriptVM, GetLastMap, "Returns the last map recorded by the launcher's Last Map system.");
	ScriptRegisterFunction	   (g_pScriptVM, FirstRunState, "Get or set the state of whether the first map was run or not. Set false/true = 0/1 | -1 to get state.");
	ScriptRegisterFunction	   (g_pScriptVM, CallFirstRunPrompt, "Shows the first run prompt if enabled in config.nut.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::GetConVarInt, "GetConVarInt", "Get the integer value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, GFunc::GetConVarString, "GetConVarString", "Get the string value of a ConVar.");
	ScriptRegisterFunctionNamed(g_pScriptVM, INDEXHANDLE, "PlayerIndexToPlayerHandle", "Takes the player's entity index and returns the player's script handle.");
	ScriptRegisterFunctionNamed(g_pScriptVM, CPortal_Player__RespawnPlayer, "RespawnPlayer", "Respawn the a player by their entity index.");
	ScriptRegisterFunctionNamed(g_pScriptVM, CPortal_Player__SetFlashlightState, "SetFlashlightState", "Set the flashlight for a player on or off.");

	// Set all the plugin function check bools to true and start the P2:MM VScript
	g_pScriptVM->Run("IncludeScript(\"multiplayermod/p2mm\");");
}
