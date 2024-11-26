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
	if (dev && !p2mm_developer.GetBool()) return; // Stop developer messages when p2mm_developer isn't enabled.

	// Take our log message and format any arguments it has into the message.
	va_list argptr;
	char szFormattedText[1024] = { 0 };
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	// Add a header to the log message.
	char completeMsg[1024];
	V_snprintf(completeMsg, sizeof(completeMsg), "(P2:MM PLUGIN): %s\n", szFormattedText);

	switch (level)
	{
	case 0:
		ConColorMsg(P2MM_PLUGIN_CONSOLE_COLOR, completeMsg);
		return;
	case 1:
		Warning(completeMsg);
		return;
	default:
		Warning("(P2:MM PLUGIN): P2MMLog level set outside of 0-1, \"%i\". Defaulting to level 0.\n", level);
		ConColorMsg(P2MM_PLUGIN_CONSOLE_COLOR, completeMsg);
		return;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Get the player's entity index by their userid.
//---------------------------------------------------------------------------------
int UserIDToPlayerIndex(int userid)
{
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		edict_t* pEdict = NULL;
		if (i >= 0 && i < g_pGlobals->maxEntities)
			pEdict = (edict_t*)(g_pGlobals->pEdicts + i);

		if (engineServer->GetPlayerUserId(pEdict) == userid)
			return i;
	}
	return NULL; // Return NULL if the index can't be found
}

//---------------------------------------------------------------------------------
// Purpose: Gets player username by their entity index.
//---------------------------------------------------------------------------------
const char* GetPlayerName(int playerIndex)
{
	if (playerIndex <= 0 || playerIndex > MAX_PLAYERS)
	{
		P2MMLog(0, true, "Invalid index passed to GetPlayerName: %i!", playerIndex);
		return "";
	}

	player_info_t playerinfo;
	if (!engineServer->GetPlayerInfo(playerIndex, &playerinfo))
	{
		P2MMLog(0, true, "Couldn't retrieve playerinfo of player index \"%i\" in GetPlayerName!", playerIndex);
		return "";
	}

	return playerinfo.name;
}

//---------------------------------------------------------------------------------
// Purpose: Gets the account ID component of player SteamID by the player's entity index.
//---------------------------------------------------------------------------------
int GetSteamID(int playerIndex)
{
	edict_t* pEdict = NULL;
	if (playerIndex >= 0 && playerIndex < MAX_PLAYERS)
		pEdict = (edict_t*)(g_pGlobals->pEdicts + playerIndex);

	if (!pEdict)
		return -1;

	player_info_t playerinfo;
	if (!engineServer->GetPlayerInfo(playerIndex, &playerinfo))
		return -1;

	const CSteamID* pSteamID = engineServer->GetClientSteamID(pEdict);
	if (!pSteamID || pSteamID->GetAccountID() == 0)
		return -1;

	return pSteamID->GetAccountID();
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
int GetConVarInt(const char* cvName)
{
	ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(1, false, "Could not find ConVar: \"%s\"! Returning -1!", cvName);
		return -1;
	}

	return pVar->GetInt();
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
const char* GetConVarString(const char* cvName)
{
	ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(1, false, "Could not find ConVar: \"%s\"! Returning \"\"!", cvName);
		return "";
	}

	return pVar->GetString();
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
void SetConVarInt(const char* cvName, int newValue)
{
	ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(1, false, "Could not set ConVar: \"%s\"!", cvName);
		return;
	}
	pVar->SetValue(newValue);
	return;
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
void SetConVarString(const char* cvName, const char* newValue)
{
	ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(1, false, "Could not set ConVar: \"%s\"!", cvName);
		return;
	}
	pVar->SetValue(newValue);
	return;
}


///			 UTIL Functions				\\\

//---------------------------------------------------------------------------------
// Purpose: Gets the player's base class with it's entity index. Thanks to Nanoman2525 for this.
//---------------------------------------------------------------------------------
CBasePlayer* UTIL_PlayerByIndex(int playerIndex)
{
#ifdef _WIN32
	static auto _PlayerByIndex = reinterpret_cast<CBasePlayer* (__cdecl*)(int)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 8B 4D 08 33 C0 85 C9 7E 30"));
	return _PlayerByIndex(playerIndex);
#else // Linux support TODO
	return NULL;
#endif
}

//---------------------------------------------------------------------------------
// Purpose: Show on screen message to players. msg_dest are defined macros in globals.hpp.
//---------------------------------------------------------------------------------
void UTIL_ClientPrint(CBasePlayer* player, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	static auto _ClientPrint = reinterpret_cast<void (__cdecl*)(CBasePlayer*, int, const char*, const char*, const char*, const char*, const char*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 83 EC 20 56 8B 75 08 85 F6 74 4C"));
	_ClientPrint(player, msg_dest, msg_name, param1, param2, param3, param4);
}

//---------------------------------------------------------------------------------
// Purpose: Show on text on screen just like game_text does.
//---------------------------------------------------------------------------------
void UTIL_HudMessage(CBasePlayer* pPlayer, const hudtextparms_t &textparms, const char* pMessage)
{
	static auto _HudMessage = reinterpret_cast<void(__cdecl*)(CBasePlayer*, const hudtextparms_t &, const char*)>(Memory::Scanner::Scan(SERVERDLL, "55 8B EC 83 EC 20 8D 4D ?? E8 ?? ?? ?? ?? 8B 45 ?? 8D 4D ?? 85 C0 74 ?? 50 E8 ?? ?? ?? ?? EB ?? E8 ?? ?? ?? ?? 56"));
	_HudMessage(pPlayer, textparms, pMessage);
}



///			 CBaseEntity Class Functions				\\\

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory. Thanks to Nanoman2525 for this.
//---------------------------------------------------------------------------------
void CBaseEntity__RemoveEntity(CBaseEntity* pEntity)
{
	reinterpret_cast<void (__cdecl*)(void*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "55 8B EC 57 8B 7D 08 85 FF 74 72"))(reinterpret_cast<IServerEntity*>(pEntity)->GetNetworkable());
}

//---------------------------------------------------------------------------------
// Purpose: Get's team number for the supplied CBasePlayer.
//---------------------------------------------------------------------------------
int CBaseEntity__GetTeamNumber(CBasePlayer* pPlayer)
{
	static auto _GetTeamNumber = reinterpret_cast<int (__thiscall*)(CBaseEntity*)>(Memory::Scanner::Scan<void*>(SERVERDLL, "8B 81 F4 02 00 00 C3"));
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
		P2MMLog(1, false , "Could not get script instance for entity!");
		return nullptr;
	}

	return _GetScriptInstance(entity);
}

///			 CBasePlayer/CPortal_Player Class Functions				\\\



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

// Array of the Gelocity maps
std::vector<MapParams> gelocityMaps =
{
	{"workshop/596984281130013835/mp_coop_gelocity_1_v02", "Gelocity 1", 1},
	{"workshop/594730048530814099/mp_coop_gelocity_2_v01", "Gelocity 2", 2},
	{"workshop/613885499245125173/mp_coop_gelocity_3_v02", "Gelocity 3", 3}
};

// Check if the host is in a Gelocity map.
// Returns which Gelocity workshop is being played.
// Returns 0 if not in a Gelocity map.
MapParams* InGelocityMap()
{
	for (int i = 0; i < gelocityMaps.size(); i++)
	{
		if (FStrEq(gelocityMaps[i].filename, CURMAPNAME)) return &gelocityMaps[i];
	}
	return NULL;
}

// Array of Portal 2 single player campaign maps.
std::vector<MapParams> spCampaignMaps =
{
	{"sp_a1_intro1",                 "Container Ride",       1},
	{"sp_a1_intro2",                 "Portal Carousel",      1},
	{"sp_a1_intro3",                 "Portal Gun",           1},
	{"sp_a1_intro4",                 "Smooth Jazz",          1},
	{"sp_a1_intro5",                 "Cube Momentum",        1},
	{"sp_a1_intro6",                 "Future Starter",       1},
	{"sp_a1_intro7",                 "Secret Panel",         1},
	{"sp_a1_wakeup",                 "Wakeup",               1},
	{"sp_a2_intro",                  "Incinerator",          1},
	{"sp_a2_laser_intro",            "Laser Intro",          2},
	{"sp_a2_laser_stairs",           "Laser Stairs",         2},
	{"sp_a2_dual_lasers",            "Dual Lasers",          2},
	{"sp_a2_laser_over_goo",         "Laser Over Goo",       2},
	{"sp_a2_catapult_intro",         "Catapult Intro",       2},
	{"sp_a2_trust_fling",            "Trust Fling",          2},
	{"sp_a2_pit_flings",             "Pit Flings",           2},
	{"sp_a2_fizzler_intro",          "Fizzler Intro",        2},
	{"sp_a2_sphere_peek",            "Ceiling Catapult",     3},
	{"sp_a2_ricochet",               "Ricochet",             3},
	{"sp_a2_bridge_intro",           "Bridge Intro",         3},
	{"sp_a2_bridge_the_gap",         "Bridge The Gap",       3},
	{"sp_a2_turret_intro",           "Turret Intro",         3},
	{"sp_a2_laser_relays",           "Laser Relays",         3},
	{"sp_a2_turret_blocker",         "Turret Blocker",       3},
	{"sp_a2_laser_vs_turret",        "Laser vs Turret",      3},
	{"sp_a2_pull_the_rug",           "Pull the Rug",         3},
	{"sp_a2_column_blocker",         "Column Blocker",       4},
	{"sp_a2_laser_chaining",         "Laser Chaining",       4},
	{"sp_a2_triple_laser",           "Triple Laser",         4},
	{"sp_a2_bts1",                   "Jailbreak",            4},
	{"sp_a2_bts2",                   "Escape",               4},
	{"sp_a2_bts3",                   "Turret Factory",       5},
	{"sp_a2_bts4",                   "Turret Sabotage",      5},
	{"sp_a2_bts5",                   "Neurotoxin Sabotage",  5},
	{"sp_a2_bts6",                   "Tube Ride",            5},
	{"sp_a2_core",                   "Core",                 5},
	{"sp_a3_00",                     "Long Fall",            6},
	{"sp_a3_01",                     "Underground",          6},
	{"sp_a3_03",                     "Cave Johnson",         6},
	{"sp_a3_jump_intro",             "Repulsion Intro",      6},
	{"sp_a3_bomb_flings",            "Bomb Flings",          6},
	{"sp_a3_crazy_box",              "Crazy Box",            6},
	{"sp_a3_transition01",           "PotatOS",              6},
	{"sp_a3_speed_ramp",             "Propulsion Intro",     7},
	{"sp_a3_speed_flings",           "Propulsion Flings",    7},
	{"sp_a3_portal_intro",           "Conversion Intro",     7},
	{"sp_a3_end",                    "Three Gels",           7},
	{"sp_a4_intro",                  "Test",                 8},
	{"sp_a4_tb_intro",               "Funnel Intro",         8},
	{"sp_a4_tb_trust_drop",          "Ceiling Button",       8},
	{"sp_a4_tb_wall_button",         "Wall Button",          8},
	{"sp_a4_tb_polarity",            "Polarity",             8},
	{"sp_a4_tb_catch",               "Funnel Catch",         8},
	{"sp_a4_stop_the_box",           "Stop the Box",         8},
	{"sp_a4_laser_catapult",         "Laser Catapult",       8},
	{"sp_a4_laser_platform",         "Laser Platform",       8},
	{"sp_a4_speed_tb_catch",         "Propulsion Catch",     8},
	{"sp_a4_jump_polarity",          "Repulsion Polarity",   8},
	{"sp_a4_finale1",                "Finale 1",             9},
	{"sp_a4_finale2",                "Finale 2",             9},
	{"sp_a4_finale3",                "Finale 3",             9},
	{"sp_a4_finale4",                "Finale 4",             9},
	{"sp_a5_credits",				 "Credits",				 10}
};

// Array of cooperative campaign maps.
std::vector<MapParams> mpCampaignMaps =
{
	{"mp_coop_start",                "Calibration",          0},
	{"mp_coop_lobby_2",              "Cooperative Lobby",    0},
	{"mp_coop_lobby_3",              "Cooperative Lobby",    0},
	{"mp_coop_doors",                "Doors",                1},
	{"mp_coop_race_2",               "Buttons",              1},
	{"mp_coop_laser_2",              "Lasers",               1},
	{"mp_coop_rat_maze",             "Rat Maze",             1},
	{"mp_coop_laser_crusher",        "Laser Crusher",        1},
	{"mp_coop_teambts",              "Behind the Scenes",    1},
	{"mp_coop_fling_3",              "Flings",               2},
	{"mp_coop_infinifling_train",    "Infinifling",          2},
	{"mp_coop_come_along",           "Team Retrieval",       2},
	{"mp_coop_fling_1",              "Vertical Flings",      2},
	{"mp_coop_catapult_1",           "Catapults",            2},
	{"mp_coop_multifling_1",         "Multifling",           2},
	{"mp_coop_fling_crushers",       "Fling Crushers",       2},
	{"mp_coop_fan",                  "Industrial Fan",       2},
	{"mp_coop_wall_intro",           "Cooperative Bridges",  3},
	{"mp_coop_wall_2",               "Bridge Swap",          3},
	{"mp_coop_catapult_wall_intro",  "Fling Block",          3},
	{"mp_coop_wall_block",           "Catapult Block",       3},
	{"mp_coop_catapult_2",           "Bridge Fling",         3},
	{"mp_coop_turret_walls",         "Turret Walls",         3},
	{"mp_coop_turret_ball",          "Turret Assassin",      3},
	{"mp_coop_wall_5",               "Bridge Testing",       3},
	{"mp_coop_tbeam_redirect",       "Cooperative Funnels",  4},
	{"mp_coop_tbeam_drill",          "Funnel Drill",         4},
	{"mp_coop_tbeam_catch_grind_1",  "Funnel Catch Coop",    4},
	{"mp_coop_tbeam_laser_1",        "Funnel Laser",         4},
	{"mp_coop_tbeam_polarity",       "Cooperative Polarity", 4},
	{"mp_coop_tbeam_polarity2",      "Funnel Hop",           4},
	{"mp_coop_tbeam_polarity3",      "Advanced Polarity",    4},
	{"mp_coop_tbeam_maze",           "Funnel Maze",          4},
	{"mp_coop_tbeam_end",            "Turret Warehouse",     4},
	{"mp_coop_paint_come_along",     "Repulsion Jumps",      5},
	{"mp_coop_paint_redirect",       "Double Bounce",        5},
	{"mp_coop_paint_bridge",         "Bridge Repulsion",     5},
	{"mp_coop_paint_walljumps",      "Wall Repulsion",       5},
	{"mp_coop_paint_speed_fling",    "Propulsion Crushers",  5},
	{"mp_coop_paint_red_racer",      "Turret Ninja",         5},
	{"mp_coop_paint_speed_catch",    "Propulsion Retrieval", 5},
	{"mp_coop_paint_longjump_intro", "Vault Entrance",       5},
	{"mp_coop_separation_1",         "Separation",           6},
	{"mp_coop_tripleaxis",           "Triple Axis",          6},
	{"mp_coop_catapult_catch",       "Catapult Catch",       6},
	{"mp_coop_2paints_1bridge",      "Bridge Gels",          6},
	{"mp_coop_paint_conversion",     "Maintenance",          6},
	{"mp_coop_bridge_catch",         "Bridge Catch",         6},
	{"mp_coop_laser_tbeam",          "Double Lift",          6},
	{"mp_coop_paint_rat_maze",       "Gel Maze",             6},
	{"mp_coop_paint_crazy_box",      "Crazier Box",          6},
};

// Checks if the host is in a singleplayer or cooperative campaign map.
// Setting mpMaps to true checks through the 
// Returns the map file name, chapter/branch number, and chapter/branch name.
// Returns NULL if not in a singleplayer or cooperative campaign map.
MapParams* InP2CampaignMap(bool mpMaps)
{
	if (mpMaps)
	{
		for (int i = 0; i < mpCampaignMaps.size(); i++)
		{
			if (FStrEq(mpCampaignMaps[i].filename, CURMAPNAME)) return &mpCampaignMaps[i];
		}
	}
	else
	{
		for (int i = 0; i < spCampaignMaps.size(); i++)
		{
			if (FStrEq(spCampaignMaps[i].filename, CURMAPNAME)) return &spCampaignMaps[i];
		}
	}
	return NULL;
}

// Array of Portal Stories: Mel campaign maps.
std::vector<MapParams> melStoryCampaignMaps =
{
	{"st_a1_tramride",      "Tram Ride",			1},
	{"st_a1_mel_intro",     "Mel Intro",			1},
	{"st_a1_lift",          "Lift",					1},
	{"st_a1_garden",        "Garden",				1},
	{"st_a2_garden_de",     "Destroyed Garden",		2},
	{"st_a2_underbounce",   "Underbounce",			2},
	{"st_a2_once_upon",     "Once Upon",			2},
	{"st_a2_past_power",    "Past Power",			2},
	{"st_a2_ramp",          "Ramp",					2},
	{"st_a2_firestorm",     "Firestorm",			2},
	{"st_a3_junkyard",      "Junkyard",				3},
	{"st_a3_concepts",      "Concepts",				3},
	{"st_a3_paint_fling",   "Paint Fling",			3},
	{"st_a3_faith_plate",   "Faith Plate",			3},
	{"st_a3_transition",    "Transition",			3},
	{"st_a4_overgrown",     "Overgrown",			4},
	{"st_a4_tb_over_goo",   "Funnel Over Goo",		4},
	{"st_a4_two_of_a_kind", "Two of a Kind",		4},
	{"st_a4_destroyed",     "Destroyed",			4},
	{"st_a4_factory",       "Factory",				4},
	{"st_a4_core_access",   "Core Access",			5},
	{"st_a4_finale",        "Finale",				5}
};

std::vector<MapParams> melAdvancedCampaignMaps =
{
	{"sp_a1_tramride",      "Tram Ride Advanced",			1},
	{"sp_a1_mel_intro",     "Mel Intro Advanced",			1},
	{"sp_a1_lift",          "Lift Advanced",				1},
	{"sp_a1_garden",        "Garden Advanced",				1},
	{"sp_a2_garden_de",     "Destroyed Garden Advanced",	2},
	{"sp_a2_underbounce",   "Underbounce Advanced",			2},
	{"sp_a2_once_upon",     "Once Upon Advanced",			2},
	{"sp_a2_past_power",    "Past Power Advanced",			2},
	{"sp_a2_ramp",          "Ramp Advanced",				2},
	{"sp_a2_firestorm",     "Firestorm Advanced",			2},
	{"sp_a3_junkyard",      "Junkyard Advanced",			3},
	{"sp_a3_concepts",      "Concepts Advanced",			3},
	{"sp_a3_paint_fling",   "Paint Fling Advanced",			3},
	{"sp_a3_faith_plate",   "Faith Plate Advanced",			3},
	{"sp_a3_transition",    "Transition Advanced",			3},
	{"sp_a4_overgrown",     "Overgrown Advanced",			4},
	{"sp_a4_tb_over_goo",   "Funnel Over Goo Advanced",		4},
	{"sp_a4_two_of_a_kind", "Two of a Kind Advanced",		4},
	{"sp_a4_destroyed",     "Destroyed Advanced",			4},
	{"sp_a4_factory",       "Factory Advanced",				4},
	{"sp_a4_core_access",   "Core Access Advanced",			5},
	{"sp_a4_finale",        "Finale Advanced",				5}
};

// Check to see which Mel map is being played.
MapParams* InMelCampaignMap(bool advanced)
{
	if (advanced)
	{
		for (int i = 0; i < melAdvancedCampaignMaps.size(); i++)
		{
			if (FStrEq(melAdvancedCampaignMaps[i].filename, CURMAPNAME)) return &melAdvancedCampaignMaps[i];
		}
	}
	else
	{
		for (int i = 0; i < melStoryCampaignMaps.size(); i++)
		{
			if (FStrEq(melStoryCampaignMaps[i].filename, CURMAPNAME)) return &melStoryCampaignMaps[i];
		}
	}
	return NULL;
}
