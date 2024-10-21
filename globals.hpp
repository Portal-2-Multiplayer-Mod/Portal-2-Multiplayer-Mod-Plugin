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

#ifdef _WIN32
#pragma once
#include <Windows.h>
#endif

#include <sstream>

class CBasePlayer;

#define P2MM_PLUGIN_CONSOLE_COLOR Color(100, 192, 252, 255)
#define P2MM_VSCRIPT_CONSOLE_COLOR Color(110, 247, 76, 255)

#define CURRENTMAPNAME STRING(gpGlobals->mapname)

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

namespace GFunc {
	int					UserIDToPlayerIndex(int userid);
	const char*			GetPlayerName(int index);
	int					GetSteamID(int index);
	int					GetConVarInt(const char* cvname);
	const char*			GetConVarString(const char* cvname);
	inline const char*	GetGameMainDir();
	inline const char*	GetGameBaseDir();
}

HSCRIPT CBaseEntity__GetScriptScope(CBaseEntity* entity);
HSCRIPT CBaseEntity__GetScriptInstance(CBaseEntity* entity);

CBasePlayer* CBasePlayer__PlayerIndexToPlayer(int playerIndex);

// If String Equals String helper function
inline bool FStrEq(const char* sz1, const char* sz2)
{
	return (V_stricmp(sz1, sz2) == 0);
}

// If String Has Substring helper function
inline bool FSubStr(const char* sz1, const char* search)
{
	return (V_strstr(sz1, search));
}

//---------------------------------------------------------------------------------
// Purpose: Entity edict to entity index. Taken from utils.h.
//---------------------------------------------------------------------------------
inline int ENTINDEX(edict_t* pEdict)
{
	if (!pEdict)
		return 0;
	int edictIndex = pEdict - gpGlobals->pEdicts;
	Assert(edictIndex < MAX_EDICTS && edictIndex >= 0);
	return edictIndex;
}

//---------------------------------------------------------------------------------
// Purpose: Entity index to entity edict. Taken from utils.h.
//---------------------------------------------------------------------------------
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

//---------------------------------------------------------------------------------
// Purpose: Entity index to script handle.
//---------------------------------------------------------------------------------
inline HSCRIPT INDEXHANDLE(int iEdictNum) {
	edict_t* pEdict = INDEXENT(iEdictNum);
	CBaseEntity* p_baseEntity = pEdict->GetUnknown()->GetBaseEntity();
	if (!p_baseEntity)
	{
		return nullptr;
	}
	HSCRIPT entityHandle = CBaseEntity__GetScriptInstance(p_baseEntity);
	return entityHandle;
}

// Get the main game directory being used. Ex. portal2/portal_stories
inline const char* GFunc::GetGameMainDir()
{
	return CommandLine()->ParmValue("-game", CommandLine()->ParmValue("-defaultgamedir", "portal2"));
}

// Get base game directory. Ex. Portal 2/Portal Stories Mel
inline const char* GFunc::GetGameBaseDir()
{
	char baseDir[MAX_PATH];
	std::string fullGameDirectoryPath = engineClient->GetGameDirectory();
	size_t firstSlash = fullGameDirectoryPath.find_last_of("\\");
	size_t secondSlash = fullGameDirectoryPath.find_last_of("\\", firstSlash - 1);
	std::string tempBaseDir = fullGameDirectoryPath.substr(secondSlash + 1, firstSlash - secondSlash - 1);
	V_strcpy(baseDir, tempBaseDir.c_str());
	return baseDir;
}
