//===========================================================================//
//
// Author: Orsell
// Purpose: Interfaced functions and hooks from the Portal 2 engine for the plugin to use.
// 
//===========================================================================//
#include "sdk.hpp"

#include "scanner.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

///			 Interfaced UTIL Functions			\\\

//---------------------------------------------------------------------------------
// Purpose: Gets the player's base class with it's entity index. Thanks to Nanoman2525 for this.
//---------------------------------------------------------------------------------
CBasePlayer* UTIL_PlayerByIndex(int playerIndex)
{
#ifdef _WIN32
	static auto _PlayerByIndex = reinterpret_cast<CBasePlayer * (__cdecl*)(int)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 8B 4D 08 33 C0 85 C9 7E 30"));
	return _PlayerByIndex(playerIndex);
#else // Linux support TODO
	return NULL;
#endif
}

//---------------------------------------------------------------------------------
// Purpose: Show on screen message to players. msg_dest are definSed macros in globals.hpp.
//---------------------------------------------------------------------------------
void UTIL_ClientPrint(CBasePlayer* player, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	static auto _ClientPrint = reinterpret_cast<void(__cdecl*)(CBasePlayer*, int, const char*, const char*, const char*, const char*, const char*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 83 EC 20 56 8B 75 08 85 F6 74 4C"));
	_ClientPrint(player, msg_dest, msg_name, param1, param2, param3, param4);
}

//---------------------------------------------------------------------------------
// Purpose: Show on text on screen just like game_text does.
//---------------------------------------------------------------------------------
void UTIL_HudMessage(CBasePlayer* pPlayer, const HudMessageParams& textparms, const char* pMessage)
{
	static auto _HudMessage = reinterpret_cast<void(__cdecl*)(CBasePlayer*, const HudMessageParams&, const char*)>(Memory::Scanner::Scan(SERVERDLL, "55 8B EC 83 EC 20 8D 4D ?? E8 ?? ?? ?? ?? 8B 45 ?? 8D 4D ?? 85 C0 74 ?? 50 E8 ?? ?? ?? ?? EB ?? E8 ?? ?? ?? ?? 56"));
	_HudMessage(pPlayer, textparms, pMessage);
}


///			 CBaseEntity Class Functions				\\\

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory. Thanks to Nanoman2525 for this.
//---------------------------------------------------------------------------------
void CBaseEntity__RemoveEntity(CBaseEntity* pEntity)
{
	reinterpret_cast<void(__cdecl*)(void*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 57 8B 7D 08 85 FF 74 72"))(reinterpret_cast<IServerEntity*>(pEntity)->GetNetworkable());
}

//---------------------------------------------------------------------------------
// Purpose: Get's team number for the supplied CBasePlayer.
//---------------------------------------------------------------------------------
int CBaseEntity__GetTeamNumber(CBasePlayer* pPlayer)
{
	static auto _GetTeamNumber = reinterpret_cast<int(__thiscall*)(CBaseEntity*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "8B 81 F4 02 00 00 C3"));
	return _GetTeamNumber((CBaseEntity*)pPlayer);
}

//---------------------------------------------------------------------------------
// Purpose: Get the script scope of a entity. Thanks to Nullderef/Vista for this.
//---------------------------------------------------------------------------------
HSCRIPT CBaseEntity__GetScriptScope(CBaseEntity* entity)
{
	if (entity == NULL)
		return NULL;

	return *reinterpret_cast<HSCRIPT*>(reinterpret_cast<uintptr_t>(entity) + 0x33c);
}

//---------------------------------------------------------------------------------
// Purpose: Get the script instance of a entity. Thanks to Nullderef/Vista for this.
//---------------------------------------------------------------------------------
HSCRIPT CBaseEntity__GetScriptInstance(CBaseEntity* entity)
{
	static auto _GetScriptInstance = reinterpret_cast<HSCRIPT(__thiscall*)(CBaseEntity*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 51 56 8B F1 83 BE 50"));
	if (!_GetScriptInstance)
	{
		P2MMLog(1, false, "Could not get script instance for entity!");
		return nullptr;
	}

	return _GetScriptInstance(entity);
}


///			 CBasePlayer Class Functions				\\\

CON_COMMAND_F(testscore, "testscore", FCVAR_HIDDEN)
{
	CBasePlayer__ShowViewPortPanel(V_atoi(args.Arg(1)), args.Arg(2));
}

void CBasePlayer__ShowViewPortPanel(int playerIndex, const char* name, bool bShow, KeyValues* data)
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, false, "Couldn't get player to display view port panel to! playerIndex: %i", playerIndex);
		return;
	}
	static auto _ShowViewPortPanel = reinterpret_cast<void(__thiscall*)(CBasePlayer*, const char*, bool, KeyValues*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 83 EC 20 53 56 8B F1 57 8D 4D ?? E8 ?? ?? ?? ?? 56"));
	_ShowViewPortPanel(pPlayer, name, bShow, data);
}


///			 CPortal_Player Class Functions				\\\

//---------------------------------------------------------------------------------
// Purpose: Respawn the a player by their entity index.
//---------------------------------------------------------------------------------
void CPortal_Player__RespawnPlayer(int playerIndex)
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, false, "Couldn't get player to respawn! playerIndex: %i", playerIndex);
		return;
	}

	static auto _RespawnPlayer = reinterpret_cast<void(__thiscall*)(CPortal_Player*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "0F 57 C0 56 8B F1 57 8D 8E"));
	_RespawnPlayer((CPortal_Player*)pPlayer);
}

//---------------------------------------------------------------------------------
// Purpose: Set the flashlight for a player on or off. Thanks to Nanoman2525 for this.
//			Not a function in the CPortal_Player class, just more grouping it with the
//			class. This does the same thing as the FlashlightTurnOn and FlashlightTurnOff
//			functions in CPortal_Player but done in one function.
//---------------------------------------------------------------------------------
void CPortal_Player__SetFlashlightState(int playerIndex, bool enable)
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(playerIndex);
	if (!pPlayer)
	{
		P2MMLog(1, true, "Couldn't get player to set flashlight state! playerIndex: %i enable: %i", playerIndex, !!enable);
		return;
	}

	if (enable)
		reinterpret_cast<void(__thiscall*)(CBaseEntity*, int)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 53 8B D9 8B 83 A8"))((CBaseEntity*)pPlayer, EF_DIMLIGHT);
	else
		reinterpret_cast<void(__thiscall*)(CBaseEntity*, int)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 53 56 8B 75 08 8B D9 8B 83"))((CBaseEntity*)pPlayer, EF_DIMLIGHT);
}