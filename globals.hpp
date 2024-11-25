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
#include "public/filesystem.h"
#include "tier2/fileutils.h"
#include "irecipientfilter.h"

#include "scanner.hpp"

#ifdef _WIN32
#pragma once
#include <Windows.h>
#endif

#include <sstream>

// Stand in class definitions.
class CBasePlayer;
class CPortal_Player;

// Color macros for game chat and console printing.
#define P2MM_PLUGIN_CONSOLE_COLOR  Color(100, 192, 252, 255) // Light Blue
#define P2MM_VSCRIPT_CONSOLE_COLOR Color(110, 247, 76, 255)  // Light Green
#define P2MM_DISCORD_CONSOLE_COLOR_NORMAL Color(88, 101, 242, 255)
#define P2MM_DISCORD_CONSOLE_COLOR_WARNING Color(255, 187, 28, 255)

#define CURMAPNAME STRING(g_pGlobals->mapname)
#define MAX_PLAYERS g_pGlobals->maxClients

// Used for autocomplete console commands.
#define COMMAND_COMPLETION_MAXITEMS		64
#define COMMAND_COMPLETION_ITEM_LENGTH	64

// A macro to iterate through all ConVars and ConCommand in the game.
// Thanks to Nanoman2525 for this.
#define FOR_ALL_CONSOLE_COMMANDS(pCommandVarName) \
    ConCommandBase *m_pConCommandList = *reinterpret_cast<ConCommandBase **>((uintptr_t)g_pCVar + 0x30); /* CCvar::m_pConCommandList */ \
    for (ConCommandBase *pCommandVarName = m_pConCommandList; \
	pCommandVarName; pCommandVarName = *reinterpret_cast<ConCommandBase **>(reinterpret_cast<uintptr_t>(pCommandVarName) + 0x04)) /* ConCommandBase::m_pNext (private variable) */

// Macro to iterate through all players.
#define FOR_ALL_PLAYERS(i) \
	for (int i = 1; i < MAX_PLAYERS; i++)

// Player team enum.
enum
{
	TEAM_SINGLEPLAYER = 0,
	TEAM_SPECTATOR,
	TEAM_RED,  
	TEAM_BLUE
};

// ClientPrint msg_dest macros.
#define HUD_PRINTNOTIFY		1 // Works same as HUD_PRINTCONSOLE
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4

// UTIL_HudMessage message parameters struct. Taken from utils.h.
// See Valve Developer Community for game_text to see which field does what:
// https://developer.valvesoftware.com/wiki/Game_text
typedef struct hudtextparms_s
{
	float		x;
	float		y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime;
	float		fadeoutTime;
	float		holdTime;
	float		fxTime;
	int			channel;
} hudtextparms_t;


//---------------------------------------------------------------------------------
// Any ConVars or CON_COMMANDS that need to be globally available.
//---------------------------------------------------------------------------------
extern ConVar p2mm_developer;

//---------------------------------------------------------------------------------
// Interfaces from the engine.
//---------------------------------------------------------------------------------
extern IVEngineServer* engineServer;
extern IVEngineClient* engineClient;
extern CGlobalVars* g_pGlobals;
extern IPlayerInfoManager* playerinfomanager;
extern IScriptVM* g_pScriptVM;
extern IServerTools* g_pServerTools;
extern IGameEventManager2* gameeventmanager_;
extern IServerPluginHelpers* pluginHelpers;
extern IFileSystem* g_pFileSystem;

// Logging function.
void P2MMLog(int level, bool dev, const char* pMsgFormat, ...);

//---------------------------------------------------------------------------------
// Interfaced game functions.
//---------------------------------------------------------------------------------
int					UserIDToPlayerIndex(int userid);
const char*			GetPlayerName(int playerIndex);
int					GetSteamID(int playerIndex);
int					GetConVarInt(const char* cvName);
const char*			GetConVarString(const char* cvName);
void				SetConVarInt(const char* cvName, int newValue);
void				SetConVarString(const char* cvName, const char* newValue);
inline const char*	GetGameMainDir();
inline const char*	GetGameBaseDir();

//---------------------------------------------------------------------------------
// Interfaced game functions.
//---------------------------------------------------------------------------------
// UTIL functions
CBasePlayer* UTIL_PlayerByIndex(int playerIndex);
void UTIL_ClientPrint(CBasePlayer* player, int msg_dest, const char* msg_name, const char* param1 = NULL, const char* param2 = NULL, const char* param3 = NULL, const char* param4 = NULL);
void UTIL_HudMessage(CBasePlayer* pPlayer, const hudtextparms_t& textparms, const char* pMessage);

// CBaseEntity functions
void CBaseEntity__RemoveEntity(CBaseEntity* pEntity);
int CBaseEntity__GetTeamNumber(CBasePlayer* pPlayer);
HSCRIPT CBaseEntity__GetScriptScope(CBaseEntity* entity);
HSCRIPT CBaseEntity__GetScriptInstance(CBaseEntity* entity);

// CPortal_Player functions
void CPortal_Player__RespawnPlayer(int playerIndex);
void CPortal_Player__SetFlashlightState(int playerIndex, bool enable);

//---------------------------------------------------------------------------------
// Player recipient filter.
//---------------------------------------------------------------------------------
class CFilter : public IRecipientFilter
{
public:
	CFilter() { recipient_count = 0; };
	~CFilter() {};

	virtual bool IsReliable() const { return false; }
	virtual bool IsInitMessage() const { return false; }

	virtual int GetRecipientCount() const { return recipient_count; }
	virtual int GetRecipientIndex(int slot) const {
		return (slot < 0 || slot >= recipient_count) ? -1 : recipients[slot];
	}

	void AddPlayer(int playerIndex)
	{
		if (recipient_count < 256)
		{
			recipients[recipient_count] = playerIndex;
			recipient_count++;
		}
	}

private:
	int recipients[256];
	int recipient_count;
};

// If String Equals String helper function. Taken from utils.h.
inline bool FStrEq(const char* sz1, const char* sz2)
{
	return (V_stricmp(sz1, sz2) == 0);
}

// If String Has Substring helper function. Taken from utils.h.
inline bool FSubStr(const char* sz1, const char* search)
{
	return (V_strstr(sz1, search));
}

// Get the current player count on the server
inline int CURPLAYERCOUNT() {
	int playerCount = 0;
	FOR_ALL_PLAYERS(i)
	{
		if (UTIL_PlayerByIndex(i))
		{
			playerCount++;
		}
	}
	return playerCount;
}

//---------------------------------------------------------------------------------
// Purpose: Entity edict to entity index. Taken from utils.h.
//---------------------------------------------------------------------------------
inline int EDICTINDEX(edict_t* pEdict)
{
	if (!pEdict)
		return 0;
	int edictIndex = pEdict - g_pGlobals->pEdicts;
	Assert(edictIndex < MAX_EDICTS && edictIndex >= 0);
	return edictIndex;
}

//---------------------------------------------------------------------------------
// Purpose: Entity to entity index.
//---------------------------------------------------------------------------------
inline int ENTINDEX(CBaseEntity* pEnt)
{
	static auto _ENTINDEX = reinterpret_cast<int (__cdecl*)(CBaseEntity*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 8B 45 ?? 85 C0 74 ?? 8B 40 ?? 85 C0 74 ?? 8B 0D"));
	return _ENTINDEX(pEnt);
}

//---------------------------------------------------------------------------------
// Purpose: Entity index to entity edict. Taken from utils.h.
//---------------------------------------------------------------------------------
inline edict_t* INDEXENT(int iEdictNum)
{
	Assert(iEdictNum >= 0 && iEdictNum < MAX_EDICTS);
	if (g_pGlobals->pEdicts)
	{
		edict_t* pEdict = g_pGlobals->pEdicts + iEdictNum;
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
	if (!pEdict->GetUnknown())
		return NULL;
	CBaseEntity* pBaseEntity = pEdict->GetUnknown()->GetBaseEntity();
	if (!pBaseEntity)
		return NULL;
	HSCRIPT entityHandle = CBaseEntity__GetScriptInstance(pBaseEntity);
	return entityHandle;
}

// Get the main game directory being used. Ex. portal2
inline const char* GetGameMainDir()
{
	return CommandLine()->ParmValue("-game", CommandLine()->ParmValue("-defaultgamedir", "portal2"));
}

// Get base game directory. Ex. Portal 2
inline const char* GetGameBaseDir()
{
	char baseDir[MAX_PATH] = { 0 };
	std::string fullGameDirectoryPath = engineClient->GetGameDirectory();
	size_t firstSlash = fullGameDirectoryPath.find_last_of("\\");
	size_t secondSlash = fullGameDirectoryPath.find_last_of("\\", firstSlash - 1);
	std::string tempBaseDir = fullGameDirectoryPath.substr(secondSlash + 1, firstSlash - secondSlash - 1);
	V_strcpy(baseDir, tempBaseDir.c_str());
	return baseDir;
}
