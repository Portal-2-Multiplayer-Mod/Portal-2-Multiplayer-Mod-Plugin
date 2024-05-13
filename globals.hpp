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


#define P2MM_CONSOLE_COLOR Color(0, 148, 100)

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

// useful helper func
inline bool FStrEq(const char* sz1, const char* sz2)
{
	return (Q_stricmp(sz1, sz2) == 0);
}