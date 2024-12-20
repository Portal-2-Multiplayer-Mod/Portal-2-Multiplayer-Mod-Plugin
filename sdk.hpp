//===========================================================================//
//
// Author: Orsell
// Purpose: Interfaced functions and hooks from the Portal 2 engine for the plugin to use.
// 
//===========================================================================//
#pragma once

#include "globals.hpp"

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
// Interfaced game functions.
//---------------------------------------------------------------------------------
// UTIL functions
CBasePlayer* UTIL_PlayerByIndex(int playerIndex);
void UTIL_ClientPrint(CBasePlayer* player, int msg_dest, const char* msg_name, const char* param1 = nullptr, const char* param2 = nullptr, const char* param3 = nullptr, const char* param4 = nullptr);
void UTIL_HudMessage(CBasePlayer* pPlayer, const hudtextparms_t& textparms, const char* pMessage);

// CBaseEntity functions
void CBaseEntity__RemoveEntity(CBaseEntity* pEntity);
int CBaseEntity__GetTeamNumber(CBasePlayer* pPlayer);
HSCRIPT CBaseEntity__GetScriptScope(CBaseEntity* entity);
HSCRIPT CBaseEntity__GetScriptInstance(CBaseEntity* entity);

// CBasePlayer functions
void CBasePlayer__ShowViewPortPanel(int playerIndex, const char* name, bool bShow = true, KeyValues* data = nullptr);

// CPortal_Player functions
void CPortal_Player__RespawnPlayer(int playerIndex);
void CPortal_Player__SetFlashlightState(int playerIndex, bool enable);