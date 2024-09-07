//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//

#include "eiface.h"
#include "cdll_int.h"
#include "igameevents.h"
#include "vscript/ivscript.h"
#include "game/server/iplayerinfo.h"
#include "engine/iserverplugin.h"
#include "public/localize/ilocalize.h"
#include "public/steam/steamclientpublic.h"

#include "scanner.hpp"
#include "modules.hpp"

#ifdef _WIN32
#pragma once
#include <Windows.h>
#endif

#include <sstream>

#define P2MM_PLUGIN_CONSOLE_COLOR Color(100, 192, 252, 255)
#define P2MM_VSCRIPT_CONSOLE_COLOR Color(110, 247, 76, 255)

#define CURMAPNAME STRING(gpGlobals->mapname)

#if _WIN32
#define MIMIMIMI(ms) Sleep(ms); // MIMIMIMI *snore* MIMIMIMI
#else
#define MIMIMIMI(ms) usleep(ms); // MIMIMIMI *snores in Linux* MIMIMIMI
#endif

class CBasePlayer;

//---------------------------------------------------------------------------------
// Any ConVars or CON_COMMANDS that need to be globally available
//---------------------------------------------------------------------------------
extern ConVar p2mm_developer;

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
extern IVEngineServer* engineServer;
extern IVEngineClient* engineClient;
extern CGlobalVars* gpGlobals;
extern IPlayerInfoManager* playerinfomanager;
extern IScriptVM* g_pScriptVM;
extern IServerTools* g_pServerTools;
extern IGameEventManager2* gameeventmanager_;
extern IServerPluginHelpers* pluginHelpers;

void P2MMLog(int level, bool dev, const char* pMsgFormat, ...);
void ReplacePattern(std::string target_module, std::string patternBytes, std::string replace_with);

namespace GFunc {
	int UserIDToPlayerIndex(int userid);
	HSCRIPT GetScriptScope(CBaseEntity* entity);
	HSCRIPT GetScriptInstance(CBaseEntity* entity);
	CBasePlayer* PlayerIndexToPlayer(int playerIndex);
	const char* GetPlayerName(int index);
	int GetSteamID(int index);
	void RemoveEntity(CBaseEntity* pEntity);
	int GetConVarInt(const char* cvname);
	const char* GetConVarString(const char* cvname);
}

// If String Equals String helper function
inline bool FStrEq(const char* sz1, const char* sz2)
{
	return (Q_stricmp(sz1, sz2) == 0);
}

// If String Has Substring helper function
inline bool FSubStr(const char* sz1, const char* search)
{
	return (Q_strstr(sz1, search));
}

// Helper functions taken from utils.h which involves entity to entity index and entity index to entity conversion
// Entity to entity index
inline int ENTINDEX(edict_t* pEdict)
{
	if (!pEdict)
		return 0;
	int edictIndex = pEdict - gpGlobals->pEdicts;
	Assert(edictIndex < MAX_EDICTS && edictIndex >= 0);
	return edictIndex;
}

// Entity index to entity
inline edict_t* INDEXENT(int iEdictNum)
{
	Assert(iEdictNum >= 0 && iEdictNum < MAX_EDICTS);
	if (gpGlobals->pEdicts)
	{
		edict_t* pEdict = gpGlobals->pEdicts + iEdictNum;
		if (pEdict->IsFree())
			return NULL;
		return pEdict;
	}
	return NULL;
}

// Get the current player count on the server
inline int CURPLAYERCOUNT() {
	int playerCount = 0;
	for (int i = 1; i < gpGlobals->maxClients; i++)
	{
		IPlayerInfo* playerinfo = playerinfomanager->GetPlayerInfo(INDEXENT(i));
		if (playerinfo)
		{
			playerCount++;
		}
	}
	return playerCount;
}