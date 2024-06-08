//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//

#include <string>

#include "globals.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------------
// Purpose: Logging for the plugin by adding a prefix and line break.
// Max character limit of 1024 characters.	
// level:	0 = Msg/DevMsg, 1 = Warning/DevWarning
//---------------------------------------------------------------------------------
void P2MMLog(int level, bool dev, const char* pMsgFormat, ...)
{
	va_list argptr;
	char szFormattedText[1024];
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	char completeMsg[1024];
	V_snprintf(completeMsg, sizeof(completeMsg), "(P2:MM PLUGIN): %s\n", szFormattedText);

	if (dev && !p2mm_developer.GetBool())
	{
		return;
	}

	switch (level)
	{
	case 0:
		ConColorMsg(P2MM_PLUGIN_CONSOLE_COLOR, completeMsg);
		return;
	case 1:
		Warning(completeMsg);
		return;
	default:
		Warning("(P2:MM PLUGIN): P2MMLog level set outside of 0-1, \"%i\", defaulting to ConColorMsg().\n", level);
		ConColorMsg(P2MM_PLUGIN_CONSOLE_COLOR, completeMsg);
		return;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Gets player entity index by userid.
//---------------------------------------------------------------------------------
int GetPlayerIndex(int userid)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* pEdict = NULL;
		if (i >= 0 && i < gpGlobals->maxEntities)
		{
			pEdict = (edict_t*)(gpGlobals->pEdicts + i);
		}

		if (engineServer->GetPlayerUserId(pEdict) == userid)
		{
			return i;
		}
	}
	return NULL; // Return NULL if the index can't be found
}

//---------------------------------------------------------------------------------
// Purpose: Gets player username by index.
//---------------------------------------------------------------------------------
const char* GetPlayerName(int index)
{
	if (index <= 0)
	{
		return "";
	}

	player_info_t playerinfo;
	if (!engineServer->GetPlayerInfo(index, &playerinfo))
	{
		return "";
	}

	return playerinfo.name;
}

//---------------------------------------------------------------------------------
// Purpose: Gets the account ID component of player SteamID by index.
//---------------------------------------------------------------------------------
int GetSteamID(int index)
{
	edict_t* pEdict = NULL;
	if (index >= 0 && index < gpGlobals->maxEntities)
	{
		pEdict = (edict_t*)(gpGlobals->pEdicts + index);
	}
	if (!pEdict)
	{
		return -1;
	}

	player_info_t playerinfo;
	if (!engineServer->GetPlayerInfo(index, &playerinfo))
	{
		return -1;
	}

	const CSteamID* pSteamID = engineServer->GetClientSteamID(pEdict);
	if (!pSteamID || pSteamID->GetAccountID() == 0)
	{
		return -1;
	}

	return pSteamID->GetAccountID();
}