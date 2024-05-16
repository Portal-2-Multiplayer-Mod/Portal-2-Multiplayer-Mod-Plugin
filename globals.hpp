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
extern IEngineSound* engineSound;
extern CGlobalVars* gpGlobals;
extern IPlayerInfoManager* playerinfomanager;
extern IScriptVM* g_pScriptVM;
extern IServerTools* g_pServerTools;
extern IGameEventManager2* gameeventmanager_;
extern IServerPluginHelpers* pluginHelpers;

void P2MMLog(int level, bool dev, const char* pMsg, ...);
extern int GetPlayerIndex(int userid);

// For making P2MM
const char* GetFormattedPrint(const char* pMsg);

// If String Equals String helper function
bool FStrEq(const char* sz1, const char* sz2);

// If String Has Substring helper function
bool FSubStr(const char* sz1, const char* search);

// Helper functions taken from utils.h which entity to entity index and entity index to entity conversion
// Entity to entity index
int ENTINDEX(edict_t* pEdict);

// Entity index to entity
edict_t* INDEXENT(int iEdictNum);