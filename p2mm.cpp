//===========================================================================//
//
// Author: Nanoman2525 & NULLderef
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

#include "eiface.h"
#include "filesystem.h"
#include "vscript/ivscript.h"
#include "game/server/iplayerinfo.h"
#include "public/toolframework/itoolentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
IVEngineServer				*engine				= NULL; // helper functions (messaging clients, loading content, making entities, running commands, etc)
CGlobalVars					*gpGlobals			= NULL;
IPlayerInfoManager			*playerinfomanager	= NULL;
IScriptVM					*g_pScriptVM		= NULL; // VScript support
IServerTools				*g_pServerTools		= NULL;
IGameEventManager			*gameeventmanager_	= NULL; // game events interface
#ifndef GAME_DLL
#define gameeventmanager gameeventmanager_
#endif

//---------------------------------------------------------------------------------
// The plugin is a static singleton that is exported as an interface
//---------------------------------------------------------------------------------
CP2MMServerPlugin g_P2MMServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CP2MMServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_P2MMServerPlugin );

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

const char *CP2MMServerPlugin::GetPluginDescription( void )
{
	return "Portal 2 Multiplayer Mod Server Plugin";
}

//---------------------------------------------------------------------------------
// Purpose: Logging for the plugin by adding a prefix and line break. level: 0 = Msg, 1 = Warning, 2 = Error
//---------------------------------------------------------------------------------
void P2MMLog(const tchar* pMsg, int level)
{
	std::string msgPrefix = "(P2:MM PLUGIN): ";
	std::string msg(pMsg);
	std::string completeMsg = (msgPrefix + msg + "\n");

	switch (level)
	{
	case 0:
		Msg((completeMsg).c_str());
		break;
	case 1:
		Warning((completeMsg).c_str());
		break;
	case 2:
		Error((completeMsg).c_str());
		break;
	default:
		Warning("(P2:MM PLUGIN): P2MMLog level set outside of 0-2, \"%d\", defaulting to Msg().");
		Msg((completeMsg).c_str());
		break;
	}
}

void ReplacePattern(std::string target_module, std::string patternBytes, std::string replace_with)
{
	void *addr = Memory::Scanner::Scan<void*>(Memory::Modules::Get(target_module), patternBytes);
	if (!addr)
	{
		P2MMLog("Failed to replace pattern!", 1);
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
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
bool CP2MMServerPlugin::Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	P2MMLog("Loading plugin...");

	if ( m_bPluginLoaded )
	{
		P2MMLog( "Already loaded.", 1 );
		m_bNoUnload = true;
		return false;
	}
	m_bPluginLoaded = true;

	ConnectTier1Libraries( &interfaceFactory, 1 );
	ConnectTier2Libraries( &interfaceFactory, 1 );

	engine = (IVEngineServer *)interfaceFactory( INTERFACEVERSION_VENGINESERVER, 0 );
	if ( !engine )
	{
		P2MMLog( "Unable to load engine!", 2);
		this->m_bNoUnload = true;
		return false;
	}

	gameeventmanager = (IGameEventManager *)interfaceFactory( INTERFACEVERSION_GAMEEVENTSMANAGER, 0 );
	if ( !gameeventmanager )
	{
		Warning("(P2:MM Plugin): Unable to load gameeventmanager\n");
		this->m_bNoUnload = true;
		return false;
	}

	playerinfomanager = (IPlayerInfoManager *)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, 0);
	if ( !playerinfomanager )
	{
		Warning( "(P2:MM Plugin): Unable to load playerinfomanager\n" );
		this->m_bNoUnload = true;
		return false;
	}

	g_pServerTools = (IServerTools *)gameServerFactory( VSERVERTOOLS_INTERFACE_VERSION, 0 );
	if ( !g_pServerTools )
	{
		Warning( "(P2:MM Plugin): Unable to load g_pServerTools\n" );
		this->m_bNoUnload = true;
		return false;
	}

	gpGlobals = playerinfomanager->GetGlobalVars();
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	ConVar_Register( 0 );

	gameeventmanager->AddListener(this, true);

	// Byte patches...

	// Linked portal doors event
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

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::Unload( void )
{
	P2MMLog("Unloading Plugin...");

	if ( m_bNoUnload )
	{
		m_bNoUnload = false;
		return;
	}

	gameeventmanager->RemoveListener(this);

	ConVar_Unregister( );
	DisconnectTier2Libraries( );
	DisconnectTier1Libraries( );
	
	m_bPluginLoaded = false;
}

void CP2MMServerPlugin::SetCommandClient( int index )
{
	m_iClientCommandIndex = index;
}

void RegisterFuncsAndRun();
void CP2MMServerPlugin::ServerActivate( edict_t* pEdictList, int edictCount, int clientMax )
{
	RegisterFuncsAndRun();
}

void CP2MMServerPlugin::FireGameEvent( KeyValues *event )
{
	if (Q_strcmp(event->GetName(), "player_say") == 0)
	{
		if (g_pScriptVM)
		{
			int entindex = NULL;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				edict_t *pEdict = NULL;
				if ( i >= 0 && i < gpGlobals->maxEntities )
				{
					pEdict = (edict_t *)( gpGlobals->pEdicts + i );
				}

				if (engine->GetPlayerUserId(pEdict) == event->GetInt("userid"))
				{
					entindex = i;
				}
			}
			const char *eventtext = event->GetString("text");

			if (entindex != NULL)
			{
				HSCRIPT func = g_pScriptVM->LookupFunction("ChatCommands");
				if (func)
				{
					g_pScriptVM->Call<int, const char*>(func, NULL, true, NULL, entindex, eventtext);
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------
// Purpose: Unused callbacks
//---------------------------------------------------------------------------------
#pragma region UNUSED_CALLBACKS
void CP2MMServerPlugin::Pause( void ) {}
void CP2MMServerPlugin::UnPause( void ) {}
void CP2MMServerPlugin::LevelInit( char const* pMapName ) {}
void CP2MMServerPlugin::GameFrame( bool simulating ) {}
void CP2MMServerPlugin::LevelShutdown( void ) {}
void CP2MMServerPlugin::ClientActive( edict_t *pEntity ) {}
void CP2MMServerPlugin::ClientDisconnect( edict_t *pEntity ) {}
void CP2MMServerPlugin::ClientPutInServer( edict_t *pEntity, char const *playername ) {}
void CP2MMServerPlugin::ClientSettingsChanged( edict_t *pEdict ) {}
PLUGIN_RESULT CP2MMServerPlugin::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::ClientFullyConnect( edict_t *pEntity ) { return; }
PLUGIN_RESULT CP2MMServerPlugin::ClientCommand( edict_t* pEntity, const CCommand& args ) { return PLUGIN_CONTINUE; }
PLUGIN_RESULT CP2MMServerPlugin::NetworkIDValidated( const char* pszUserName, const char* pszNetworkID ) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue ) {}
void CP2MMServerPlugin::OnEdictAllocated( edict_t *edict ) {}
void CP2MMServerPlugin::OnEdictFreed( const edict_t *edict  ) {}
bool CP2MMServerPlugin::BNetworkCryptKeyCheckRequired( uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey ) { return false; }
bool CP2MMServerPlugin::BNetworkCryptKeyValidate( uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte *pbEncryptedBufferFromClient, byte *pbPlainTextKeyForNetchan ) { return true; }
#pragma endregion