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

#define P2MM_PLUGIN_VERSION "1.1" // Update this when a new version of the plugin is released
#define P2MM_VERSION "2.3" // Update this for whatever P2:MM Version it's released with

#define P2MM_CONSOLE_COLOR Color(0, 148, 100)


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
extern IGameEventManager* gameeventmanager_;
extern IServerPluginHelpers* helpers;

void P2MMLog(int level, bool dev, const tchar* pMsg, ...);
int UserIDToEntityIndex(int userid);

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