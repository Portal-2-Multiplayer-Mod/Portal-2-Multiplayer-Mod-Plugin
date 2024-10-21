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
int GFunc::UserIDToPlayerIndex(int userid)
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
const char* GFunc::GetPlayerName(int index)
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
int GFunc::GetSteamID(int index)
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

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
void GFunc::RemoveEntity(CBaseEntity* pEntity)
{
	reinterpret_cast<void (*)(void*)>(Memory::Scanner::Scan<void*>(Memory::Modules::Get("server"), "55 8B EC 57 8B 7D 08 85 FF 74 72"))(reinterpret_cast<IServerEntity*>(pEntity)->GetNetworkable());
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
int GFunc::GetConVarInt(const char* cvname)
{
	ConVar* pVar = g_pCVar->FindVar(cvname);
	if (!pVar)
	{
		P2MMLog(1, false, "Could not find ConVar: \"%s\"! Returning -1!", cvname);
		return -1;
	}

	return pVar->GetInt();
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
const char* GFunc::GetConVarString(const char* cvname)
{
	ConVar* pVar = g_pCVar->FindVar(cvname);
	if (!pVar)
	{
		P2MMLog(1, false, "Could not find ConVar: \"%s\"! Returning \"\"!", cvname);
		return "";
	}

	return pVar->GetString();
}

///			 CBaseEntity Class Functions				\\\

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory. Thanks to Nanoman2525 for this.
//---------------------------------------------------------------------------------
void CBaseEntity__RemoveEntity(CBaseEntity* pEntity)
{
	//reinterpret_cast<IServerEntity*>(pEntity) trust me bro aka, we know its CBaseEntity*, but we want the IServerEntity* so cast to that to get its methods 
	reinterpret_cast<void (*)(void*)>(Memory::Scanner::Scan<void*>(Memory::Modules::Get("server"), "55 8B EC 57 8B 7D 08 85 FF 74 72"))(reinterpret_cast<IServerEntity*>(pEntity)->GetNetworkable());
}

//---------------------------------------------------------------------------------
// Purpose: Get the script scope of a entity. Thanks to Nullderef/Vista for this.
//---------------------------------------------------------------------------------
HSCRIPT CBaseEntity__GetScriptScope(CBaseEntity* entity)
{
	if (entity == NULL)
	{
		return NULL;
	}
	// Returing class variable of the script scope, offset being 0x33c
	// Because we have the pointer to the scope, dereference it to get the member variable/function of the class, but in turn the type is now a pointer and the whole thing needs to be derefernenced
	// offset the reinterpret_case and the return type
	// 
	return *reinterpret_cast<HSCRIPT*>(reinterpret_cast<uintptr_t>(entity) + 0x33c);
}

//---------------------------------------------------------------------------------
// Purpose: Get the script instance of a entity. Thanks to Nullderef/Vista for this.
//---------------------------------------------------------------------------------
HSCRIPT CBaseEntity__GetScriptInstance(CBaseEntity* entity)
{
	static auto _GetScriptInstance = reinterpret_cast<HSCRIPT(__thiscall*)(CBaseEntity*)>(Memory::Scanner::Scan<void*>(Memory::Modules::Get("server"), "55 8B EC 51 56 8B F1 83 BE 50"));
	if (!_GetScriptInstance) {
		P2MMLog(1, false, "GetScriptEntity not found");
		return nullptr;
	}

	return _GetScriptInstance(entity);
}



///			 CPortal_Player/CBasePlayer Class Functions				\\\

//---------------------------------------------------------------------------------
// Purpose: Gets player base class by player entity index. Thanks to Nanoman2525 for this.
//---------------------------------------------------------------------------------
CBasePlayer* CBasePlayer__PlayerIndexToPlayer(int playerIndex)
{
#ifdef _WIN32
	static auto _PlayerIndexToPlayer = reinterpret_cast<CBasePlayer * (__cdecl*)(int)>(Memory::Scanner::Scan<void*>(Memory::Modules::Get("server"), "55 8B EC 8B 4D 08 33 C0 85 C9 7E 30"));
	return _PlayerIndexToPlayer(playerIndex);
#else // Linux support TODO
	return NULL;
#endif
}

