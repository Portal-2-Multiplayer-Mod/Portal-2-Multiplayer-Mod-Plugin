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

#include "filesystem.h"
#include "public/toolframework/itoolentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
IVEngineServer* engineServer = NULL; // Access engine server helper functions (messaging clients, loading content, making entities, running commands, etc)
IVEngineClient* engineClient = NULL; // Access engine client helper functions
CGlobalVars* gpGlobals = NULL; // Access global variables shared between the engine and games dlls
IPlayerInfoManager* playerinfomanager = NULL; // Access functions for players
IScriptVM* g_pScriptVM = NULL; // VScript support
IServerTools* g_pServerTools = NULL; // Access to interface from engine to tools for manipulating entities
IGameEventManager* gameeventmanager_ = NULL; // Game events interface
IServerPluginHelpers* helpers = NULL; // Helper plugin functions
#ifndef GAME_DLL
#define gameeventmanager gameeventmanager_
#endif

//---------------------------------------------------------------------------------
// The plugin is a static singleton that is exported as an interface
//---------------------------------------------------------------------------------
CP2MMServerPlugin g_P2MMServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CP2MMServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_P2MMServerPlugin);

ConVar p2mm_developer("p2mm_developer", "0", FCVAR_NONE, "Enable for P2:MM developer messages.");
ConVar p2mm_lastmap("p2mm_lastmap", "", FCVAR_NONE, "Last map recorded for the Last Map system.");
ConVar p2mm_firstrun("p2mm_firstrun", "1", FCVAR_NONE, "Flag for checking if it's the first map run for the session. Manual modification not recommended as it can mess things up.");
ConVar p2mm_splitscreen("p2mm_splitscreen", "0", FCVAR_NONE, "Flag for the main menu buttons to start in splitscreen or not.");

CON_COMMAND(p2mm_startsession, "Starts up a P2:MM session with the defined map and whether it should be splitscreen or not.")
{
	// Make sure the CON_COMMAND was executed correctly
	if (args.ArgC() < 2)
	{
		P2MMLog(1, false, "p2mm_startsession called incorrectly!");
		P2MMLog(1, false, "Usage: 'p2mm_startsession (map to start)'.");
		return;
	}

	// A check done by the menu ot request to use the 
	const char* requestedMap = args.Arg(1);
	if (requestedMap == "P2MM_LASTMAP")
	{
		requestedMap = p2mm_lastmap.GetString();
	}

	// Check if the supplied map is a valid map
	if (!engineServer->IsMapValid(requestedMap.c_str()))
	{
		P2MMLog(1, false, "p2mm_startsession was given a non-valid map! %s", requestedMap);
		return;
	}

	// Check if the user requested it to start in splitscreen or not
	// Whether we are starting a singleplayer or multiplayer map,
	// it will start at mp_coop_community_hub so the mod gets started correctly
	const char* startSessionCommand = "";
	if (p2mm_splitscreen.GetBool())
	{
		startSessionCommand = "ss_map mp_coop_community_hub";
	}
	else
	{
		startSessionCommand = "map mp_coop_community_hub";
	}

	// Set first run ConVar flag on and set the last map ConVar value so the system
	// can change from mp_coop_community_hub to the requested map
	p2mm_firstrun.SetValue(1);
	p2mm_lastmap.SetValue(requestedMap);
	engine->ServerCommand(startSessionCommand);
}

//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::CP2MMServerPlugin()
{
	this->m_iClientCommandIndex = 0;

	// Store plugin Status
	this->m_bPluginLoaded = false;
	this->m_bNoUnload = false; // If we fail to load, we don't want to run anything on Unload()
}

CP2MMServerPlugin::~CP2MMServerPlugin()
{
}

const char* CP2MMServerPlugin::GetPluginDescription(void)
{
	return "Portal 2: Multiplayer Mod Server Plugin";
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
// Purpose: Called when the plugin is loaded, initialization process. Loads the interfaces we need from the engine and applies our patches.
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

	gameeventmanager = (IGameEventManager*)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER, 0);
	if (!gameeventmanager)
	{
		P2MMLog(1, false, "Unable to load gameeventmanager!");
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

	g_pServerTools = (IServerTools*)gameServerFactory(VSERVERTOOLS_INTERFACE_VERSION, 0);
	if (!g_pServerTools)
	{
		P2MMLog(1, false, "Unable to load g_pServerTools!");
		this->m_bNoUnload = true;
		return false;
	}

	gpGlobals = playerinfomanager->GetGlobalVars();
	MathLib_Init(2.2f, 2.2f, 0.0f, 2.0f);
	ConVar_Register(0);

	gameeventmanager->AddListener(this, true);

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

	P2MMLog(0, false, "Loaded plugin!");
	m_bPluginLoaded = true;
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is turning off/unloading. Currently causes the game to crash no clue why.
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

//---------------------------------------------------------------------------------
// Purpose: Capture and work with game events. Interfaces game events to VScript functions.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::FireGameEvent(KeyValues* event)
{
	// Event called when a player inputs a message into the chat, "player_say" returns:
	/*
		"userid"	"short"		// user ID on server
		"text"		"string"	// the say text
	*/
	if (FStrEq(event->GetName(), "player_say"))
	{
		if (g_pScriptVM)
		{
			int userid = event->GetInt("userid");
			const char* text = event->GetString("text");
			int entindex = UserIDToEntityIndex(userid);

			if (entindex != NULL)
			{
				// Handling chat commands
				HSCRIPT cc_func = g_pScriptVM->LookupFunction("ChatCommands");
				if (cc_func)
				{
					g_pScriptVM->Call<const char*, int>(cc_func, NULL, true, NULL, text, entindex);
				}

				// Handle VScript interface function
				HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSay");
				if (ge_func)
				{
					g_pScriptVM->Call<int, const char*>(ge_func, NULL, true, NULL, userid, text, entindex);
				}
			}
		}
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
PLUGIN_RESULT CP2MMServerPlugin::ClientCommand(edict_t* pEntity, const CCommand& args) { return PLUGIN_CONTINUE; }
PLUGIN_RESULT CP2MMServerPlugin::NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) {}
void CP2MMServerPlugin::OnEdictAllocated(edict_t* edict) {}
void CP2MMServerPlugin::OnEdictFreed(const edict_t* edict) {}
bool CP2MMServerPlugin::BNetworkCryptKeyCheckRequired(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey) { return false; }
bool CP2MMServerPlugin::BNetworkCryptKeyValidate(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte* pbEncryptedBufferFromClient, byte* pbPlainTextKeyForNetchan) { return true; }
void CP2MMServerPlugin::CreateMessage(edict_t* pEntity, DIALOG_TYPE type, KeyValues* data, IServerPluginCallbacks* plugin) {}
void CP2MMServerPlugin::ClientCommand(edict_t* pEntity, const char* cmd) {}
QueryCvarCookie_t CP2MMServerPlugin::StartQueryCvarValue(edict_t* pEntity, const char* pName) {return QueryCvarCookie_t();}
#pragma endregion