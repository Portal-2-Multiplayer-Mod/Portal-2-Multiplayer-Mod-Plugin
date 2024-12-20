//===========================================================================//
//
// Author: Orsell
// Purpose: Interfaced functions and hooks from the Portal 2 engine for the plugin to use.
// 
//===========================================================================//
#pragma once

#include "globals.hpp"

// UTIL_HudMessage message parameters struct. Based on the one from util.h.
// See Valve Developer Community for game_text to see which field does what:
// https://developer.valvesoftware.com/wiki/Game_text
typedef struct
{
	float		x, y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime, fadeoutTime, holdTime;
	float		fxTime;
	int			channel;
} HudMessageParams;

//---------------------------------------------------------------------------------
// Hooked game functions.
//---------------------------------------------------------------------------------
// NoSteamLogon Stop Hook.
extern void (__fastcall* CSteam3Server__OnGSClientDenyHelper_orig)(CSteam3Server* thisptr, void* edx, CBaseClient* cl, void* eDenyReason, const char* pchOptionalText);
void __fastcall CSteam3Server__OnGSClientDenyHelper_hook(CSteam3Server* thisptr, void* edx, CBaseClient* cl, void* eDenyReason, const char* pchOptionalText);

// Model Replacement Hooks.
extern const char* (__cdecl* GetBallBotModel_orig)(bool bLowRes);
const char* GetBallBotModel_hook(bool bLowRes);

extern const char* (__cdecl* GetEggBotModel_orig)(bool bLowRes);
const char* GetEggBotModel_hook(bool bLowRes);

extern const char* (__fastcall* CPortal_Player__GetPlayerModelName_orig)(CPortal_Player* thisptr);
const char* __fastcall CPortal_Player__GetPlayerModelName_hook(CPortal_Player* thisptr);

// Respawn Hooks.
extern void (__fastcall* CPortal_Player__PlayerDeathThink_orig)(CPortal_Player* thisptr);
void __fastcall CPortal_Player__PlayerDeathThink_hook(CPortal_Player* thisptr);

extern void (__cdecl* respawn_orig)(CBaseEntity* pEdict, bool fCopyCorpse);
void __cdecl respawn_hook(CBaseEntity* pEdict, bool fCopyCorpse);

//---------------------------------------------------------------------------------
// Interfaced game functions.
//---------------------------------------------------------------------------------
// UTIL functions
CBasePlayer* UTIL_PlayerByIndex(int playerIndex);
void UTIL_ClientPrint(CBasePlayer* player, int msg_dest, const char* msg_name, const char* param1 = nullptr, const char* param2 = nullptr, const char* param3 = nullptr, const char* param4 = nullptr);
void UTIL_HudMessage(CBasePlayer* pPlayer, const HudMessageParams& textparms, const char* pMessage);

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