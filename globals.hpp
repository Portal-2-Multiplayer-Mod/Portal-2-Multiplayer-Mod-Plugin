//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//

#include "eiface.h"
#include "cdll_int.h"
#include "engine/IEngineSound.h"
#include "igameevents.h"
#include "vscript/ivscript.h"
#include "game/server/iplayerinfo.h"
#include "engine/iserverplugin.h"

#define P2MM_CONSOLE_COLOR Color(0, 148, 100, 255)

//---------------------------------------------------------------------------------
// Any ConVars or CON_COMMANDS that need to be globally available
//---------------------------------------------------------------------------------
extern ConVar p2mm_developer;

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
extern IVEngineServer* engineServer;
extern IVEngineClient* engineClient;
extern IEngineSound* engineSound;
extern CGlobalVars* gpGlobals;
extern IPlayerInfoManager* playerinfomanager;
extern IScriptVM* g_pScriptVM;
extern IServerTools* g_pServerTools;
extern IGameEventManager2* gameeventmanager_;
extern IServerPluginHelpers* pluginHelpers;

void P2MMLog(int level, bool dev, const char* pMsg, ...);

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

// Helper functions taken from utils.h which entity to entity index and entity index to entity conversion
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

extern int GetPlayerIndex(int userid);
