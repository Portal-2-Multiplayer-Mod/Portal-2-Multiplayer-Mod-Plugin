//===========================================================================//
//
// Author: Nanoman2525 & NULLderef
// Maintainer: Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
// Note: This plugin was made in like 20 mins, but everything works.
// 
//===========================================================================//

#include "p2mm.hpp"
#include "scanner.hpp"
#include "modules.hpp"

#ifdef _WIN32
#pragma once
#include <Windows.h>
#endif

#include <sstream>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
IVEngineServer* engineServer = NULL; // Access engine server functions (messaging clients, loading content, making entities, running commands, etc)
IVEngineClient* engineClient = NULL; // Access engine client functions
CGlobalVars* gpGlobals = NULL; // Access global variables shared between the engine and games dlls
IPlayerInfoManager* playerinfomanager = NULL; // Access interface functions for players
IScriptVM* g_pScriptVM = NULL; // Access VScript interface
IServerTools* g_pServerTools = NULL; // Access to interface from engine to tools for manipulating entities
IGameEventManager2* gameeventmanager_ = NULL; // Access game events interface
IServerPluginHelpers* pluginHelpers = NULL; // Access interface for plugin helper functions
ILocalize* localize = NULL; // Access locatize interface to access localization files
#ifndef GAME_DLL
#define gameeventmanager gameeventmanager_
#endif

// List of game events the plugin interfaces used to load each one
const char* gameevents[] =
{
	"portal_player_touchedground",
	"portal_player_ping",
	"portal_player_portaled",
	"turret_hit_turret",
	"security_camera_detached",
	"player_say"
};

//---------------------------------------------------------------------------------
// The plugin is a static singleton that is exported as an interface
//---------------------------------------------------------------------------------
CP2MMServerPlugin g_P2MMServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CP2MMServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_P2MMServerPlugin);

ConVar p2mm_developer("p2mm_developer", "0", FCVAR_NONE, "Enable for P2:MM developer messages.");
ConVar p2mm_lastmap("p2mm_lastmap", "", FCVAR_HIDDEN, "Last map recorded for the Last Map system.");
ConVar p2mm_splitscreen("p2mm_splitscreen", "0", FCVAR_HIDDEN, "Flag for the main menu buttons to start in splitscreen or not.");
//ConVar p2mm_loop("p2mm_loop", "0", FCVAR_DONE)

CON_COMMAND(p2mm_startsession, "Starts up a P2:MM session with a requested map.")
{
	// Make sure the CON_COMMAND was executed correctly
	if (args.ArgC() < 2 || FStrEq(args.Arg(1), ""))
	{
		P2MMLog(1, false, "p2mm_startsession called incorrectly! Usage: 'p2mm_startsession (map to start) '");
		return;
	}

	// A check done by the menu to request to use the last recorded map in the p2mm_lastmap ConVar
	std::string requestedMap = args.Arg(1);
	P2MMLog(0, true, "Requested Map: %s", requestedMap.c_str());
	if (FSubStr(requestedMap.c_str(), "P2MM_LASTMAP"))
	{
		P2MMLog(0, true, "p2mm_lastmap: %s", p2mm_lastmap.GetString());
		if (!engineServer->IsMapValid(p2mm_lastmap.GetString()))
		{
			P2MMLog(1, false, "p2mm_session was called with P2MM_LASTMAP, but p2mm_lastmap is empty or invalid!");
			engineClient->ExecuteClientCmd("disconnect \"There is no last map recorded or it doesn't exist! Please start a play session with the other options first.\"");
			engineClient->ExecuteClientCmd("playvol \"music/mainmenu/portal2_background01\" 0.35");
			return;
		}
		requestedMap = p2mm_lastmap.GetString();
		P2MMLog(0, true, "P2MM_LASTMAP called! Running Last Map: %s", requestedMap.c_str());
	}
	p2mm_lastmap.SetValue(""); // Set last map ConVar to blank so it doesn't trigger in situations where we don't want it to trigger

	// Check if the supplied map is a valid map
	if (!engineServer->IsMapValid(requestedMap.c_str()))
	{
		P2MMLog(1, false, "p2mm_startsession was given a non-valid map or one that doesn't exist! %s", requestedMap.c_str());
		return;
	}

	// Check if the user requested it to start in splitscreen or not
	std::string mapString = "";
	if (p2mm_splitscreen.GetBool())
	{
		mapString = "ss_map ";
	}
	else
	{
		mapString = "map ";
	}
	P2MMLog(0, true, "Map String: %s", mapString.c_str());

	// Set first run ConVar flag on and set the last map ConVar value so the system
	// can change from mp_coop_community_hub to the requested map.
	// Also set m_bSeenFirstRunPrompt back to false so the prompt can be triggered again.
	g_P2MMServerPlugin.m_bFirstMapRan = true;
	g_P2MMServerPlugin.m_bSeenFirstRunPrompt = false;
	if (!FSubStr(requestedMap.c_str(), "mp_coop"))
	{
		P2MMLog(0, true, "'mp_coop' not found, singleplayer map being run.");
		p2mm_lastmap.SetValue(requestedMap.c_str());
		engineClient->ExecuteClientCmd(std::string(mapString + "mp_coop_community_hub").c_str());
	}
	else
	{
		P2MMLog(0, true, "'mp_coop' found, multiplayer map being run.");
		engineClient->ExecuteClientCmd(std::string(mapString + requestedMap).c_str());
	}
}

void ReplacePattern(std::string target_module, std::string patternBytes, std::string replace_with)
{
	void* addr = Memory::Scanner::Scan<void*>(Memory::Modules::Get(target_module), patternBytes);
	if (!addr)
	{
		P2MMLog(1, false, "Failed to replace pattern!");
		return;
	}

	std::vector<uint8_t> replace;

	std::istringstream patternStream(replace_with);
	std::string patternByte;
	while (patternStream >> patternByte)
	{
		replace.push_back((uint8_t)std::stoul(patternByte, nullptr, 16));
	}

	DWORD oldprotect = 0;
	DWORD newprotect = PAGE_EXECUTE_READWRITE;
	VirtualProtect(addr, replace.size(), newprotect, &oldprotect);
	memcpy_s(addr, replace.size(), replace.data(), replace.size());
	VirtualProtect(addr, replace.size(), oldprotect, &newprotect);
}

//---------------------------------------------------------------------------------
// Purpose: constructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::CP2MMServerPlugin()
{
	this->m_iClientCommandIndex = 0;

	// Store plugin Status
	this->m_bPluginLoaded = false;
	this->m_bNoUnload = false;				// If we fail to load, we don't want to run anything on Unload()

	// Store game vars
	this->m_bSeenFirstRunPrompt = false;	// Flag is set true after CallFirstRunPrompt() is called in VScript
	this->m_bFirstMapRan = true;			// Checks if the game ran for the first time

	m_nDebugID = EVENT_DEBUG_ID_INIT;
}

//---------------------------------------------------------------------------------
// Purpose: destructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::~CP2MMServerPlugin()
{
	m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
}

const char* CP2MMServerPlugin::GetPluginDescription(void)
{
	static std::string pluginDescription = "Portal 2: Multiplayer Mod Server Plugin | Plugin Version: " + std::string(P2MM_PLUGIN_VERSION) + " | For P2:MM Version: " + std::string(P2MM_VERSION);
	return pluginDescription.c_str();
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is loaded, initialization process.
//			Loads the interfaces we need from the engine and applies our patches.
//---------------------------------------------------------------------------------
bool CP2MMServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	if (m_bPluginLoaded)
	{
		P2MMLog(1, false, "Already loaded!");
		m_bNoUnload = true;
		return false;
	}

	P2MMLog(0, false, "Loading plugin...");

	ConnectTier1Libraries(&interfaceFactory, 1);
	ConnectTier2Libraries(&interfaceFactory, 1);

	// Make sure that all the interfaces needed are loaded and useable

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

	playerinfomanager = (IPlayerInfoManager*)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, 0);
	if (!playerinfomanager)
	{
		P2MMLog(1, false, "Unable to load playerinfomanager!");
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

	gameeventmanager = (IGameEventManager2*)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2, 0);
	if (!gameeventmanager)
	{
		P2MMLog(1, false, "Unable to load gameeventmanager!");
		this->m_bNoUnload = true;
		return false;
	}

	pluginHelpers = (IServerPluginHelpers*)interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, 0);
	if (!pluginHelpers)
	{
		P2MMLog(1, false, "Unable to load pluginHelpers!");
		this->m_bNoUnload = true;
		return false;
	}

	localize = (ILocalize*)interfaceFactory(LOCALIZE_INTERFACE_VERSION, 0);
	if (!localize)
	{
		P2MMLog(1, false, "Unable to load localize!");
		this->m_bNoUnload = true;
		return false;
	}

	gpGlobals = playerinfomanager->GetGlobalVars();
	MathLib_Init(2.2f, 2.2f, 0.0f, 2.0f);
	ConVar_Register(0);

	// Add listener for all used game events
	for (const char* gameevent : gameevents)
	{
		gameeventmanager->AddListener(this, gameevent, true);
	}
	
	// Byte patches

	// Linked portal doors event crash patch
	ReplacePattern("server", "0F B6 87 04 05 00 00 8B 16", "EB 14 87 04 05 00 00 8B 16");

	// Partner disconnects
	ReplacePattern("server", "51 50 FF D2 83 C4 10 E8", "51 50 90 90 83 C4 10 E8");
	ReplacePattern("server", "74 28 3B 75 FC", "EB 28 3B 75 FC");

	// Max players -> 33
	ReplacePattern("server", "83 C0 02 89 01", "83 C0 20 89 01");
	ReplacePattern("engine", "85 C0 78 13 8B 17", "31 C0 04 21 8B 17");
	static uintptr_t sv = *reinterpret_cast<uintptr_t*>(Memory::Scanner::Scan<void*>(Memory::Modules::Get("engine"), "74 0A B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B E5", 3));
	*reinterpret_cast<int*>(sv + 0x228) = 33;

	// Prevent disconnect by "STEAM validation rejected"
	ReplacePattern("engine", "01 74 7D 8B", "01 EB 7D 8B");

	// Fix sv_password
	ReplacePattern("engine", "0F 95 C1 51 8D 4D E8", "03 C9 90 51 8D 4D E8");

	// Make sure -allowspectators is there so we get our 33 max players
	if (!CommandLine()->FindParm("-allowspectators"))
	{
		CommandLine()->AppendParm("-allowspectators", "");
	}

	P2MMLog(0, false, "Loaded plugin!");
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

	gameeventmanager->RemoveListener(this);

	ConVar_Unregister();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();

	// Undo byte patches

	// Linked portal doors event crash patch
	ReplacePattern("server", "EB 14 87 04 05 00 00 8B 16", "0F B6 87 04 05 00 00 8B 16");

	// Partner disconnects
	ReplacePattern("server", "51 50 90 90 83 C4 10 E8", "51 50 FF D2 83 C4 10 E8");
	ReplacePattern("server", "EB 28 3B 75 FC", "74 28 3B 75 FC");

	// Max players -> 3
	ReplacePattern("server", "83 C0 20 89 01", "83 C0 02 89 01");
	ReplacePattern("engine", "31 C0 04 21 8B 17", "85 C0 78 13 8B 17");
	static uintptr_t sv = *reinterpret_cast<uintptr_t*>(Memory::Scanner::Scan<void*>(Memory::Modules::Get("engine"), "74 0A B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B E5", 3));
	*reinterpret_cast<int*>(sv + 0x228) = 2;

	// Disconnect by "STEAM validation rejected"
	ReplacePattern("engine", "01 EB 7D 8B", "01 74 7D 8B");

	// sv_password
	ReplacePattern("engine", "03 C9 90 51 8D 4D E8", "0F 95 C1 51 8D 4D E8");

	m_bPluginLoaded = false;
}

void CP2MMServerPlugin::SetCommandClient(int index)
{
	m_iClientCommandIndex = index;
}

void RegisterFuncsAndRun();
void CP2MMServerPlugin::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	RegisterFuncsAndRun();
}

PLUGIN_RESULT CP2MMServerPlugin::ClientCommand(edict_t* pEntity, const CCommand& args)
{
	if (!pEntity || pEntity->IsFree())
	{
		return PLUGIN_CONTINUE;
	}

	const char* pcmd = args[0];
	const char* fargs = args.ArgS();
	short userid = engineServer->GetPlayerUserId(pEntity);
	int entindex = GetPlayerIndex(userid);

	P2MMLog(0, true, "ClientCommand called: %s", pcmd);
	P2MMLog(0, true, "ClientCommand args: %s", args.ArgS());
	P2MMLog(0, true, "userid: %i", userid);
	P2MMLog(0, true, "entindex: %i", entindex);
	P2MMLog(0, true, "VScript VM Working?: %s", (g_pScriptVM != NULL) ? "Working" : "Not Working!");

	// Call the "GEClientCommand" VScript function
	if (g_pScriptVM)
	{
		HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEClientCommand");
		if (ge_func)
		{
			g_pScriptVM->Call<const char*, const char*, short, int>(ge_func, NULL, true, NULL, pcmd, fargs, userid, entindex);
		}
	}
	
	/*if (FStrEq(pcmd, ""))
	{
		return PLUGIN_CONTINUE;
	}*/

	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: Capture and work with game events. Interfaces game events to VScript functions.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::FireGameEvent(IGameEvent* event)
{
	P2MMLog(0, true, "Game Event Fired: %s", event->GetName());
	P2MMLog(0, true, "VScript VM Working?: %s", (g_pScriptVM != NULL) ? "Working" : "Not Working!");

	// Event called when a player touches the ground, "portal_player_touchedground" returns:
	/*
		"userid"	"short"		// user ID on server
	*/
	if (FStrEq(event->GetName(), "portal_player_touchedground"))
	{
		short userid = event->GetInt("userid");
		int entindex = GetPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerLanded");
			if (ge_func)
			{
				g_pScriptVM->Call<short, int>(ge_func, NULL, true, NULL, userid, entindex);
			}
		}

		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "entindex: %i", entindex);
		return;
	}
	// Event called when a player pings, "portal_player_ping" returns:
	/*
		"userid"	"short"		// user ID on server
		"ping_x"	"float"		// ping's x-coordinate in map
		"ping_y"	"float"		// ping's y-coordinate in map
		"ping_z"	"float"		// ping's z-coordinate in map
	*/
	else if (FStrEq(event->GetName(), "portal_player_ping"))
	{
		short userid = event->GetInt("userid");
		float ping_x = event->GetFloat("ping_x");
		float ping_y = event->GetFloat("ping_y");
		float ping_z = event->GetFloat("ping_z");
		int entindex = GetPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerPing");
			if (ge_func)
			{
				g_pScriptVM->Call<short, float, float, float, int>(ge_func, NULL, true, NULL, userid, ping_x, ping_y, ping_z, entindex);
			}
		}

		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "ping_x: %f", ping_x);
		P2MMLog(0, true, "ping_y: %f", ping_y);
		P2MMLog(0, true, "ping_z: %f", ping_z);
		P2MMLog(0, true, "entindex: %i", entindex);
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
		int entindex = GetPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerPortaled");
			if (ge_func)
			{
				g_pScriptVM->Call<short, bool, int>(ge_func, NULL, true, NULL, userid, portal2, entindex);
			}
		}

		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "portal2: %s", portal2 ? "true" : "false");
		P2MMLog(0, true, "entindex: %i", entindex);
		return;
	}
	// Event called when a player goes through a portal, "turret_hit_turret" returns nothing.
	else if (FStrEq(event->GetName(), "turret_hit_turret"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GETurretHitTurret");
			if (ge_func)
			{
				g_pScriptVM->Call(ge_func, NULL, true, NULL);
			}
		}
		return;
	}
	// Event called when a player goes through a portal, "security_camera_detached" returns nothing.
	else if (FStrEq(event->GetName(), "security_camera_detached"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GECamDetach");
			if (ge_func)
			{
				g_pScriptVM->Call(ge_func, NULL, true, NULL);
			}
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
		int entindex = GetPlayerIndex(userid);

		if (g_pScriptVM)
		{
			if (entindex != NULL)
			{
				// Handling chat commands
				HSCRIPT cc_func = g_pScriptVM->LookupFunction("ChatCommands");
				if (cc_func)
				{
					g_pScriptVM->Call<const char*, int>(cc_func, NULL, true, NULL, text, entindex);
				}
			}
			
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSay");
			if (ge_func)
			{
				g_pScriptVM->Call<short, const char*, int>(ge_func, NULL, true, NULL, userid, text, entindex);
			}
			
		}

		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "text: %s", text);
		P2MMLog(0, true, "entindex: %i", entindex);
		return;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Unused callbacks
//---------------------------------------------------------------------------------
#pragma region UNUSED_CALLBACKS
void CP2MMServerPlugin::Pause(void) {}
void CP2MMServerPlugin::UnPause(void) {}
void CP2MMServerPlugin::GameFrame(bool simulating) {}
void CP2MMServerPlugin::LevelInit(char const* pMapName) {}
void CP2MMServerPlugin::LevelShutdown(void) {}
void CP2MMServerPlugin::ClientActive(edict_t* pEntity) {}
void CP2MMServerPlugin::ClientDisconnect(edict_t* pEntity) {}
void CP2MMServerPlugin::ClientPutInServer(edict_t* pEntity, char const* playername) {}
void CP2MMServerPlugin::ClientSettingsChanged(edict_t* pEdict) {}
PLUGIN_RESULT CP2MMServerPlugin::ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::ClientFullyConnect(edict_t* pEntity) { return; }
PLUGIN_RESULT CP2MMServerPlugin::NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) {}
void CP2MMServerPlugin::OnEdictAllocated(edict_t* edict) {}
void CP2MMServerPlugin::OnEdictFreed(const edict_t* edict) {}
bool CP2MMServerPlugin::BNetworkCryptKeyCheckRequired(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey) { return false; }
bool CP2MMServerPlugin::BNetworkCryptKeyValidate(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte* pbEncryptedBufferFromClient, byte* pbPlainTextKeyForNetchan) { return true; }
#pragma endregion