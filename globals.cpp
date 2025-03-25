//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//
#include "globals.hpp"

#include "sdk.hpp"
#include "p2mm.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------------
// Purpose: Logging for the plugin by adding a prefix and line break.
// Max character limit of 1024 characters.	
// level:	0 = Msg/DevMsg, 1 = Warning/DevWarning, 2 = Error WILL STOP ENGINE!
//---------------------------------------------------------------------------------
void P2MMLog(const LogLevel level, const bool dev, const char* pMsgFormat, ...)
{
	if (dev && !p2mm_developer.GetBool() && level != ERRORR) return; // Stop developer messages when p2mm_developer isn't enabled.

	// Take our log message and format any arguments it has into the message.
	va_list argptr;
	char szFormattedText[1024] = { 0 };
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	// Add a header to the log message.
	char completeMsg[1024] = { 0 };
	V_snprintf(completeMsg, sizeof(completeMsg), "(P2:MM PLUGIN): %s\n", szFormattedText);

	switch (level)
	{
	case (INFO):
		ConColorMsg(P2MM_PLUGIN_CONSOLE_COLOR, completeMsg);
		return;
	case (WARNING):
		Warning(completeMsg);
		return;
	case (ERRORR):
		Warning("(P2:MM PLUGIN):\n!!!ERROR ERROR ERROR!!!:\nA FATAL ERROR OCCURED WITH THE ENGINE:\n%s", completeMsg);
		Error(completeMsg);
		return;
	default:
		Warning("(P2:MM PLUGIN): P2MMLog level set outside of 0-1, \"%i\". Defaulting to level INFO.\n", level);
		ConColorMsg(P2MM_PLUGIN_CONSOLE_COLOR, completeMsg);
	}
}

//---------------------------------------------------------------------------------
// Purpose: Get the player's entity index by their userid.
//---------------------------------------------------------------------------------
int UserIDToPlayerIndex(const int userid)
{
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		const edict_t* pEdict = nullptr;
		if (i >= 0 && i < g_pGlobals->maxEntities)
			pEdict = (g_pGlobals->pEdicts + i);

		if (engineServer->GetPlayerUserId(pEdict) == userid)
			return i;
	}
	return 0; // Return 0 if the index can't be found
}

//---------------------------------------------------------------------------------
// Purpose: Gets player username by their entity index.
//---------------------------------------------------------------------------------
const char* GetPlayerName(const int playerIndex)
{
	if (playerIndex <= 0 || playerIndex > MAX_PLAYERS)
	{
		P2MMLog(WARNING, true, "Invalid index passed to GetPlayerName: %i! Returning ""!", playerIndex);
		return "";
	}

	player_info_t playerInfo;
	if (!engineServer->GetPlayerInfo(playerIndex, &playerInfo))
	{
		P2MMLog(WARNING, true, R"(Couldn't retrieve playerInfo of player index in GetPlayerName: %i! Returning ""!)", playerIndex);
		return "";
	}

	return playerInfo.name;
}

//---------------------------------------------------------------------------------
// Purpose: Gets the account ID component of player SteamID by the player's entity index.
//---------------------------------------------------------------------------------
int GetSteamID(const int playerIndex)
{
	edict_t* pEdict = nullptr;
	if (playerIndex >= 0 && playerIndex < MAX_PLAYERS)
		pEdict = (edict_t*)(g_pGlobals->pEdicts + playerIndex);

	if (!pEdict)
		return -1;

	player_info_t playerInfo;
	if (!engineServer->GetPlayerInfo(playerIndex, &playerInfo))
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
	const ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(WARNING, false, R"(Could not find ConVar: "%s"! Returning ""!)", cvName);
		return -1;
	}

	return pVar->GetInt();
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
const char* GetConVarString(const char* cvName)
{
	const ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(WARNING, false, R"(Could not find ConVar: "%s"! Returning ""!)", cvName);
		return "";
	}

	return pVar->GetString();
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
void SetConVarInt(const char* cvName, const int newValue)
{
	ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(WARNING, false, "Could not set ConVar: \"%s\"!", cvName);
		return;
	}
	pVar->SetValue(newValue);
}

//---------------------------------------------------------------------------------
// Purpose: Self-explanatory.
//---------------------------------------------------------------------------------
void SetConVarString(const char* cvName, const char* newValue)
{
	ConVar* pVar = g_pCVar->FindVar(cvName);
	if (!pVar)
	{
		P2MMLog(WARNING, false, R"(Could not set ConVar: "%s"!)", cvName);
		return;
	}
	pVar->SetValue(newValue);
	return;
}

//---------------------------------------------------------------------------------
// Purpose: Returns true if player is a bot.
//---------------------------------------------------------------------------------
bool IsBot(const int playerIndex)
{
	player_info_t playerInfo;
	if (!engineServer->GetPlayerInfo(playerIndex, &playerInfo))
	{
		P2MMLog(WARNING, true, R"(Couldn't retrieve player info of player index "%i" in IsBot!)", playerIndex);
		return false;
	}

	return playerInfo.fakeplayer;
}

//---------------------------------------------------------------------------------
// Purpose: Get the number of bots in the server.
//---------------------------------------------------------------------------------
int GetBotCount()
{
	int b = 0;
	FOR_ALL_PLAYERS(i)
	{
		if (IsBot(i)) b++;
	}
	return b;
}

//---------------------------------------------------------------------------------
// Purpose: Get the current player count on the server.
//---------------------------------------------------------------------------------
int CURPLAYERCOUNT()
{
	int playerCount = 0;
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		if (UTIL_PlayerByIndex(i))
			playerCount++;
	}
	return playerCount;
}

//---------------------------------------------------------------------------------
// Purpose: Entity index to script handle.
//---------------------------------------------------------------------------------
HSCRIPT INDEXHANDLE(const int iEdictNum)
{
	edict_t* pEdict = INDEXENT(iEdictNum);
	if (!pEdict->GetUnknown())
		return nullptr;
		
	CBaseEntity* pBaseEntity = pEdict->GetUnknown()->GetBaseEntity();
	if (!pBaseEntity)
		return nullptr;
	
	return CBaseEntity__GetScriptInstance(pBaseEntity);
}

///			 Campaign Map Arrays & Functions			\\\

//---------------------------------------------------------------------------------
// GELOCITY MAPS
//---------------------------------------------------------------------------------

const std::vector<MapParams> gelocityMaps =
{
	{"workshop/596984281130013835/mp_coop_gelocity_1_v02", "Gelocity 1", 1},
	{"workshop/594730048530814099/mp_coop_gelocity_2_v01", "Gelocity 2", 2},
	{"workshop/613885499245125173/mp_coop_gelocity_3_v02", "Gelocity 3", 3}
};

// Check if the host is in a Gelocity map.
// Returns which Gelocity workshop is being played.
// Returns 0 if not in a Gelocity map.
const MapParams* InGelocityMap()
{
	for (const auto& gelocityMap : gelocityMaps)
	{
		if (FStrEq(CURMAPFILENAME, gelocityMap.mapFile))
			return &gelocityMap;
	}

	return nullptr;
}

//---------------------------------------------------------------------------------
// PORTAL 2 SINGLE PLAYER CAMPAIGN
//---------------------------------------------------------------------------------
const std::vector<MapParams> SP_CAMPAIGN_MAPS =
{
	{"sp_a1_intro1",                 "Container Ride",       1,		"The Courtesy Call"},
	{"sp_a1_intro2",                 "Portal Carousel",      1,		"The Courtesy Call"},
	{"sp_a1_intro3",                 "Portal Gun",           1,		"The Courtesy Call"},
	{"sp_a1_intro4",                 "Smooth Jazz",          1,		"The Courtesy Call"},
	{"sp_a1_intro5",                 "Cube Momentum",        1,		"The Courtesy Call"},
	{"sp_a1_intro6",                 "Future Starter",       1,		"The Courtesy Call"},
	{"sp_a1_intro7",                 "Secret Panel",         1,		"The Courtesy Call"},
	{"sp_a1_wakeup",                 "Wakeup",               1,		"The Courtesy Call"},
	{"sp_a2_intro",                  "Incinerator",          1,		"The Courtesy Call"},
	{"sp_a2_laser_intro",            "Laser Intro",          2,		"The Cold Boot"},
	{"sp_a2_laser_stairs",           "Laser Stairs",         2,		"The Cold Boot"},
	{"sp_a2_dual_lasers",            "Dual Lasers",          2,		"The Cold Boot"},
	{"sp_a2_laser_over_goo",         "Laser Over Goo",       2,		"The Cold Boot"},
	{"sp_a2_catapult_intro",         "Catapult Intro",       2,		"The Cold Boot"},
	{"sp_a2_trust_fling",            "Trust Fling",          2,		"The Cold Boot"},
	{"sp_a2_pit_flings",             "Pit Flings",           2,		"The Cold Boot"},
	{"sp_a2_fizzler_intro",          "Fizzler Intro",        2,		"The Cold Boot"},
	{"sp_a2_sphere_peek",            "Ceiling Catapult",     3,		"The Return"},
	{"sp_a2_ricochet",               "Ricochet",             3,		"The Return"},
	{"sp_a2_bridge_intro",           "Bridge Intro",         3,		"The Return"},
	{"sp_a2_bridge_the_gap",         "Bridge The Gap",       3,		"The Return"},
	{"sp_a2_turret_intro",           "Turret Intro",         3,		"The Return"},
	{"sp_a2_laser_relays",           "Laser Relays",         3,		"The Return"},
	{"sp_a2_turret_blocker",         "Turret Blocker",       3,		"The Return"},
	{"sp_a2_laser_vs_turret",        "Laser vs Turret",      3,		"The Return"},
	{"sp_a2_pull_the_rug",           "Pull the Rug",         3,		"The Return"},
	{"sp_a2_column_blocker",         "Column Blocker",       4,		"The Surprise"},
	{"sp_a2_laser_chaining",         "Laser Chaining",       4,		"The Surprise"},
	{"sp_a2_triple_laser",           "Triple Laser",         4,		"The Surprise"},
	{"sp_a2_bts1",                   "Jailbreak",            4,		"The Surprise"},
	{"sp_a2_bts2",                   "Escape",               4,		"The Surprise"},
	{"sp_a2_bts3",                   "Turret Factory",       5,		"The Escape"},
	{"sp_a2_bts4",                   "Turret Sabotage",      5,		"The Escape"},
	{"sp_a2_bts5",                   "Neurotoxin Sabotage",  5,		"The Escape"},
	{"sp_a2_bts6",                   "Tube Ride",            5,		"The Escape"},
	{"sp_a2_core",                   "Core",                 5,		"The Escape"},
	{"sp_a3_00",                     "Long Fall",            6,		"The Fall"},
	{"sp_a3_01",                     "Underground",          6,		"The Fall"},
	{"sp_a3_03",                     "Cave Johnson",         6,		"The Fall"},
	{"sp_a3_jump_intro",             "Repulsion Intro",      6,		"The Fall"},
	{"sp_a3_bomb_flings",            "Bomb Flings",          6,		"The Fall"},
	{"sp_a3_crazy_box",              "Crazy Box",            6,		"The Fall"},
	{"sp_a3_transition01",           "PotatOS",              6,		"The Fall"},
	{"sp_a3_speed_ramp",             "Propulsion Intro",     7,		"The Reunion"},
	{"sp_a3_speed_flings",           "Propulsion Flings",    7,		"The Reunion"},
	{"sp_a3_portal_intro",           "Conversion Intro",     7,		"The Reunion"},
	{"sp_a3_end",                    "Three Gels",           7,		"The Reunion"},
	{"sp_a4_intro",                  "Test",                 8,		"The Itch"},
	{"sp_a4_tb_intro",               "Funnel Intro",         8,		"The Itch"},
	{"sp_a4_tb_trust_drop",          "Ceiling Button",       8,		"The Itch"},
	{"sp_a4_tb_wall_button",         "Wall Button",          8,		"The Itch"},
	{"sp_a4_tb_polarity",            "Polarity",             8,		"The Itch"},
	{"sp_a4_tb_catch",               "Funnel Catch",         8,		"The Itch"},
	{"sp_a4_stop_the_box",           "Stop the Box",         8,		"The Itch"},
	{"sp_a4_laser_catapult",         "Laser Catapult",       8,		"The Itch"},
	{"sp_a4_laser_platform",         "Laser Platform",       8,		"The Itch"},
	{"sp_a4_speed_tb_catch",         "Propulsion Catch",     8,		"The Itch"},
	{"sp_a4_jump_polarity",          "Repulsion Polarity",   8,		"The Itch"},
	{"sp_a4_finale1",                "Finale 1",             9,		"THE Part Where He Kills You"},
	{"sp_a4_finale2",                "Finale 2",             9,		"THE Part Where He Kills You"},
	{"sp_a4_finale3",                "Finale 3",             9,		"THE Part Where He Kills You"},
	{"sp_a4_finale4",                "Finale 4",             9,		"THE Part Where He Kills You"},
	{"sp_a5_credits",				   "Credits",			 	10,	    "The End"}
};

//---------------------------------------------------------------------------------
// PORTAL 2 COOPERATIVE CAMPAIGN
//---------------------------------------------------------------------------------
const std::vector<MapParams> MP_CAMPAIGN_MAPS =
{
	{"mp_coop_start",                "Calibration",          0,		"Introduction"},
	{"mp_coop_lobby_2",              "Cooperative Lobby",    0,		"Introduction"},
	{"mp_coop_lobby_3",              "Cooperative Lobby",    0,		"Introduction"},
	{"mp_coop_community_hub",        "Community Hub",		1,		"Community Hub"},
	{"mp_coop_doors",                "Doors",                1,		"Team Building"},
	{"mp_coop_race_2",               "Buttons",              1,		"Team Building"},
	{"mp_coop_laser_2",              "Lasers",               1,		"Team Building"},
	{"mp_coop_rat_maze",             "Rat Maze",             1,		"Team Building"},
	{"mp_coop_laser_crusher",        "Laser Crusher",        1,		"Team Building"},
	{"mp_coop_teambts",              "Behind the Scenes",    1,		"Team Building"},
	{"mp_coop_fling_3",              "Flings",               2,		"Mass And Velocity"},
	{"mp_coop_infinifling_train",    "Infinifling",          2,		"Mass And Velocity"},
	{"mp_coop_come_along",           "Team Retrieval",       2,		"Mass And Velocity"},
	{"mp_coop_fling_1",              "Vertical Flings",      2,		"Mass And Velocity"},
	{"mp_coop_catapult_1",           "Catapults",            2,		"Mass And Velocity"},
	{"mp_coop_multifling_1",         "Multifling",           2,		"Mass And Velocity"},
	{"mp_coop_fling_crushers",       "Fling Crushers",       2,		"Mass And Velocity"},
	{"mp_coop_fan",                  "Industrial Fan",       2,		"Mass And Velocity"},
	{"mp_coop_wall_intro",           "Cooperative Bridges",  3,		"Hard-Light Surfaces"},
	{"mp_coop_wall_2",               "Bridge Swap",          3,		"Hard-Light Surfaces"},
	{"mp_coop_catapult_wall_intro",  "Fling Block",          3,		"Hard-Light Surfaces"},
	{"mp_coop_wall_block",           "Catapult Block",       3,		"Hard-Light Surfaces"},
	{"mp_coop_catapult_2",           "Bridge Fling",         3,		"Hard-Light Surfaces"},
	{"mp_coop_turret_walls",         "Turret Walls",         3,		"Hard-Light Surfaces"},
	{"mp_coop_turret_ball",          "Turret Assassin",      3,		"Hard-Light Surfaces"},
	{"mp_coop_wall_5",               "Bridge Testing",       3,		"Hard-Light Surfaces"},
	{"mp_coop_tbeam_redirect",       "Cooperative Funnels",  4,		"Excursion Funnels"},
	{"mp_coop_tbeam_drill",          "Funnel Drill",         4,		"Excursion Funnels"},
	{"mp_coop_tbeam_catch_grind_1",  "Funnel Catch Coop",    4,		"Excursion Funnels"},
	{"mp_coop_tbeam_laser_1",        "Funnel Laser",         4,		"Excursion Funnels"},
	{"mp_coop_tbeam_polarity",       "Cooperative Polarity", 4,		"Excursion Funnels"},
	{"mp_coop_tbeam_polarity2",      "Funnel Hop",           4,		"Excursion Funnels"},
	{"mp_coop_tbeam_polarity3",      "Advanced Polarity",    4,		"Excursion Funnels"},
	{"mp_coop_tbeam_maze",           "Funnel Maze",          4,		"Excursion Funnels"},
	{"mp_coop_tbeam_end",            "Turret Warehouse",     4,		"Excursion Funnels"},
	{"mp_coop_paint_come_along",     "Repulsion Jumps",      5,		"Mobility Gels"},
	{"mp_coop_paint_redirect",       "Double Bounce",        5,		"Mobility Gels"},
	{"mp_coop_paint_bridge",         "Bridge Repulsion",     5,		"Mobility Gels"},
	{"mp_coop_paint_walljumps",      "Wall Repulsion",       5,		"Mobility Gels"},
	{"mp_coop_paint_speed_fling",    "Propulsion Crushers",  5,		"Mobility Gels"},
	{"mp_coop_paint_red_racer",      "Turret Ninja",         5,		"Mobility Gels"},
	{"mp_coop_paint_speed_catch",    "Propulsion Retrieval", 5,		"Mobility Gels"},
	{"mp_coop_paint_longjump_intro", "Vault Entrance",       5,		"Mobility Gels"},
	{"mp_coop_credits",			   "Credits",			  	5,		"The End! :D"},
	{"mp_coop_separation_1",         "Separation",           6,		"Art Therapy"},
	{"mp_coop_tripleaxis",           "Triple Axis",          6,		"Art Therapy"},
	{"mp_coop_catapult_catch",       "Catapult Catch",       6,		"Art Therapy"},
	{"mp_coop_2paints_1bridge",      "Bridge Gels",          6,		"Art Therapy"},
	{"mp_coop_paint_conversion",     "Maintenance",          6,		"Art Therapy"},
	{"mp_coop_bridge_catch",         "Bridge Catch",         6,		"Art Therapy"},
	{"mp_coop_laser_tbeam",          "Double Lift",          6,		"Art Therapy"},
	{"mp_coop_paint_rat_maze",       "Gel Maze",             6,		"Art Therapy"},
	{"mp_coop_paint_crazy_box",      "Crazier Box",          6,		"Art Therapy"}
};

// Checks if the host is in a single player or cooperative campaign map.
// Setting mpMaps to true checks through the cooperative campaign maps array.
// Returns the map file name, chapter/branch number, and chapter/branch name.
// Returns nullptr if not in a single player or cooperative campaign map.
const MapParams* InP2CampaignMap(const bool mpMaps)
{
	if (mpMaps)
	{
		for (const auto& mpCampaignMap : MP_CAMPAIGN_MAPS)
		{
			if (FStrEq(CURMAPFILENAME, mpCampaignMap.mapFile))
				return &mpCampaignMap;
		}
	}
	else
	{
		for (const auto& spCampaignMap : SP_CAMPAIGN_MAPS)
		{
			if (FStrEq(CURMAPFILENAME, spCampaignMap.mapFile))
				return &spCampaignMap;
		}
	}

	return nullptr;
}

//---------------------------------------------------------------------------------
// PORTAL STORIES: MEL CAMPAIGNS
//---------------------------------------------------------------------------------

const std::vector<MapParams> MEL_STORY_CAMPAIGN_MAPS =
{
	{"st_a1_tramride",      "Tram Ride",		1,	"1952"},
	{"st_a1_mel_intro",     "Mel Intro",		1,	"1952"},
	{"st_a1_lift",          "Lift",				1,	"1952"},
	{"st_a1_garden",        "Garden",			1,	"1952"},
	{"st_a2_garden_de",     "Destroyed Garden", 2,	"Extended Relaxation"},
	{"st_a2_underbounce",   "Underbounce",		2,	"Extended Relaxation"},
	{"st_a2_once_upon",     "Once Upon",		2,	"Extended Relaxation"},
	{"st_a2_past_power",    "Past Power",		2,	"Extended Relaxation"},
	{"st_a2_ramp",          "Ramp",				2,	"Extended Relaxation"},
	{"st_a2_firestorm",     "Firestorm",		2,	"Extended Relaxation"},
	{"st_a3_junkyard",      "Junkyard",			3,	"The Ascent"},
	{"st_a3_concepts",      "Concepts",			3,	"The Ascent"},
	{"st_a3_paint_fling",   "Paint Fling",		3,	"The Ascent"},
	{"st_a3_faith_plate",   "Faith Plate",		3,	"The Ascent"},
	{"st_a3_transition",    "Transition",		3,	"The Ascent"},
	{"st_a4_overgrown",     "Overgrown",		4,	"Organic Complications"},
	{"st_a4_tb_over_goo",   "Funnel Over Goo",	4,	"Organic Complications"},
	{"st_a4_two_of_a_kind", "Two of a Kind",	4,	"Organic Complications"},
	{"st_a4_destroyed",     "Destroyed",		4,	"Organic Complications"},
	{"st_a4_factory",       "Factory",			4,	"Organic Complications"},
	{"st_a4_core_access",   "Core Access",		5,	"INTRUSION"},
	{"st_a4_finale",        "Finale",			5,	"INTRUSION"},
	{"st_a5_credits",		  "Credits",			6,	"The End"}
};
 
const std::vector<MapParams> MEL_ADVANCED_CAMPAIGN_MAPS =
{
	{"sp_a1_tramride",      "Tram Ride (Advanced)",			1,	"1952"},
	{"sp_a1_mel_intro",     "Mel Intro (Advanced)",			1,	"1952"},
	{"sp_a1_lift",          "Lift (Advanced)",				1,	"1952"},
	{"sp_a1_garden",        "Garden (Advanced)",				1,	"1952"},
	{"sp_a2_garden_de",     "Destroyed Garden (Advanced)",	2,	"Extended Relaxation"},
	{"sp_a2_underbounce",   "Underbounce (Advanced)",		2,	"Extended Relaxation"},
	{"sp_a2_once_upon",     "Once Upon (Advanced)",			2,	"Extended Relaxation"},
	{"sp_a2_past_power",    "Past Power (Advanced)",			2,	"Extended Relaxation"},
	{"sp_a2_ramp",          "Ramp (Advanced)",				2,	"Extended Relaxation"},
	{"sp_a2_firestorm",     "Firestorm (Advanced)",			2,	"Extended Relaxation"},
	{"sp_a3_junkyard",      "Junkyard (Advanced)",			3,	"The Ascent"},
	{"sp_a3_concepts",      "Concepts (Advanced)",			3,	"The Ascent"},
	{"sp_a3_paint_fling",   "Paint Fling (Advanced)",		3,	"The Ascent"},
	{"sp_a3_faith_plate",   "Faith Plate (Advanced)",		3,	"The Ascent"},
	{"sp_a3_transition",    "Transition (Advanced)",			3,	"The Ascent"},
	{"sp_a4_overgrown",     "Overgrown (Advanced)",			4,	"Organic Complications"},
	{"sp_a4_tb_over_goo",   "Funnel Over Goo (Advanced)",	4,	"Organic Complications"},
	{"sp_a4_two_of_a_kind", "Two of a Kind (Advanced)",		4,	"Organic Complications"},
	{"sp_a4_destroyed",     "Destroyed (Advanced)",			4,	"Organic Complications"},
	{"sp_a4_factory",       "Factory (Advanced)",			4,	"Organic Complications"},
	{"sp_a4_core_access",   "Core Access (Advanced)",		5,	"INTRUSION"},
	{"sp_a4_finale",        "Finale (Advanced)",				5,	"INTRUSION"},
	{"sp_a5_credits",		  "Credits (Advanced)",			6,	"The End"}
};

// Check to see which Mel map is being played.
const MapParams* InMelCampaignMap(const bool advanced)
{
	if (advanced)
	{
		for (const auto& melAdvancedCampaignMap : MEL_ADVANCED_CAMPAIGN_MAPS)
		{
			if (FStrEq(CURMAPFILENAME, melAdvancedCampaignMap.mapFile))
				return &melAdvancedCampaignMap;
		}
	}
	else
	{
		for (const auto& melStoryCampaignMap : MEL_STORY_CAMPAIGN_MAPS)
		{
			if (FStrEq(CURMAPFILENAME, melStoryCampaignMap.mapFile))
				return &melStoryCampaignMap;
		}
	}
	
	return nullptr;
}

//---------------------------------------------------------------------------------
// APERTURE TAG CAMPAIGN
//---------------------------------------------------------------------------------
// Array of maps for Aperture Tag
const std::vector<MapParams> APERTURE_TAG_CAMPAIGN_MAPS =
{
	{"gg_intro_wakeup",	"Wake Up/Intro", 1, "Aperture Tag"},
	{"gg_blue_only",	    "Blue Only I",	  1, "Aperture Tag"},
	{"gg_blue_only_2",	"Blue Only II",  1, "Aperture Tag"},
	{"gg_blue_only_3",	"Blue Only III", 1, "Aperture Tag"},
	{"gg_blue_only_2_pt2","Blue Only IV",  1, "Aperture Tag"},
	{"gg_a1_intro4",		"Smooth Jazz",	  1, "Aperture Tag"},
	{"gg_blue_upplatform","Portals",       1, "Aperture Tag"},
	{"gg_red_only",		  "Speed Gel Upgrade", 2,"Speed Gel Upgrade"},
	{"gg_red_surf",		  "Surf",              2,"Speed Gel Upgrade"},
	{"gg_all_intro",		  "All Intro",         2,"Speed Gel Upgrade"},
	{"gg_all_rotating_wall","Rotating Wall",     2,"Speed Gel Upgrade"},
	{"gg_all_fizzler",	  "Fizzlers",          2,"Speed Gel Upgrade"},
	{"gg_all_intro_2",	  "All Intro 2",       2,"Speed Gel Upgrade"},
	{"gg_a2_column_blocker","Column Blocker",      3,"No More Recycling Tests"},
	{"gg_all_puzzle2",	  "Portals 2",           3,"No More Recycling Tests"},
	{"gg_all2_puzzle1",	  "Future Starter",      3,"No More Recycling Tests"},
	{"gg_all_puzzle1",	  "Final Qualification", 3,"No More Recycling Tests"},
	{"gg_all2_escape1",	  "ALSSER/Escape",       3,"No More Recycling Tests"},
	{"gg_stage_reveal",		 "Reveal",           4,"The Stage"},
	{"gg_stage_bridgebounce_2","Bridge Bounce",    4,"The Stage"},
	{"gg_stage_redfirst",		 "Red First",        4,"The Stage"},
	{"gg_stage_laserrelay",	 "Laser Relay",      4,"The Stage"},
	{"gg_stage_beamscotty",	 "Citranium Desert", 4,"The Stage"},
	{"gg_stage_bridgebounce",	 "Bridge Bounce 2",  4,"The Stage"},
	{"gg_stage_roofbounce",	 "Roof Bounce",      4,"The Stage"},
	{"gg_stage_pickbounce",	 "Pick Bounce",      4,"The Stage"},
	{"gg_stage_theend",		 "The End",          4,"The Stage"},
	{"gg_credit_video", "Game Credits",                   5, "Extras"},
	{"gg_trailer_map",  "Trailer Map",                    5, "Extras"},
	{"gg_tag_remix",	  "TAG: The Power of Paint Remake", 5, "Extras"}
};

// Check to see which Aperture Tag map is being played.
const MapParams* InApertureTagCampaignMap()
{
	for (const auto& apertureTagCampaignMap : APERTURE_TAG_CAMPAIGN_MAPS)
	{
		if (FStrEq(CURMAPFILENAME, apertureTagCampaignMap.mapFile))
			return &apertureTagCampaignMap;
	}
	
	return nullptr;
}

// Portal Reloaded support will not happen for some time, this will remain commented out.
// //---------------------------------------------------------------------------------
// // PORTAL RELOADED CAMPAIGNS
// //---------------------------------------------------------------------------------
// 
// const std::vector<MapParams> PORTAL_RELOADED_SP_CAMPAIGN_MAPS =
// {
// 	{"sp_a1_pr_map_001",	"Human Storage Vault",	1,	"Human Storage Vault"},
// 	{"sp_a1_pr_map_002",	"Time Travel",			2,	"Time Travel"},
// 	{"sp_a1_pr_map_003",	"Cubes and Buttons",	3,	"Cubes and Buttons"},
// 	{"sp_a1_pr_map_004",	"Portals",				4,	"Portals"},
// 	{"sp_a1_pr_map_005",	"Time Portals",			5,	"Time Portals"},
// 	{"sp_a1_pr_map_006",	"Timing Tests",			6,	"Timing Tests"},
// 	{"sp_a1_pr_map_007",	"Lasers",				7,	"Lasers"},
// 	{"sp_a1_pr_map_008",	"Aerial Faithplates",	8,	"Aerial Faithplates"},
// 	{"sp_a1_pr_map_009",	"Light Bridges",		9,	"Light Bridges"},
// 	{"sp_a1_pr_map_010",	"Turrets",				10,	"Turrets"},
// 	{"sp_a1_pr_map_011",	"Excursion Funnels",		11,	"Exursion Funnels"},
// 	{"sp_a1_pr_map_012",	"Finale",				12,	"Finale"}
// };
//
// const std::vector<MapParams> PORTAL_RELOADED_MP_CAMPAIGN_MAPS =
// {
// 	{"mp_coop_start",		"Course Selection Hub",	0,	"Course Selection Hub"},
// 	{"mp_coop_lobby_3",		"Course Selection Hub",	0,	"Course Selection Hub"},
// 	{"mp_coop_end",			"Course Selection Hub",	0,	"Course Selection Hub"},
// 	{"mp_coop_pr_cubes",	"Cube Logic",			1,	"Orientation"},
// 	{"mp_coop_pr_portals",	"Portal Logic",			1,	"Orientation"},
// 	{"mp_coop_pr_teamwork",	"Teamwork",				1,	"Orientation"},
// 	{"mp_coop_pr_fling",	"Fling",				2,	"Momentum"},
// 	{"mp_coop_pr_loop",		"Loop",					2,	"Momentum"},
// 	{"mp_coop_pr_catapult",	"Faithplates",			2,	"Momentum"},
// 	{"mp_coop_pr_laser",	"Laser",				3,	"Advanced"},
// 	{"mp_coop_pr_bridge",	"Light Bridges",		3,	"Advanced"},
// 	{"mp_coop_pr_tbeam",	"Funnel",				3,	"Advanced"},
// 	{"mp_coop_pr_bts",		"Behind The Scenes",	4,	"Finale"}
// };
//
// // Check to see which Mel map is being played.
// const MapParams* InReloadedCampaignMap(bool singeplayerCampaign)
// {
// 	if (singeplayerCampaign)
// 	{
// 		for (const auto& portalReloadedSPCampaignMap : PORTAL_RELOADED_SP_CAMPAIGN_MAPS)
// 		{
// 			if (FStrEq(CURMAPFILENAME, portalReloadedSPCampaignMap.mapFile))
// 				return &portalReloadedSPCampaignMap;
// 		}
// 	}
// 	else
// 	{
// 		for (const auto& portalReloadedMPCampaignMap : PORTAL_RELOADED_MP_CAMPAIGN_MAPS)
// 		{
// 			if (FStrEq(CURMAPFILENAME, portalReloadedMPCampaignMap.mapFile))
// 				return &portalReloadedMPCampaignMap;
// 		}
// 	}
// 	
// 	return nullptr;
// }

//---------------------------------------------------------------------------------
// PORTAL: DIVINITY CAMPAIGN
//---------------------------------------------------------------------------------

const std::vector<MapParams> DIVINITY_CAMPAIGN_MAPS =
{
	{"sp_a1_divinity_intro",			"Intro",			 1,	"House of Leaves"},
	{"sp_a1_divinity_bts1",				"BTS 1",			 1,	"House of Leaves"},
	{"sp_a1_divinity_field_intro",		"Field Intro",		 1,	"House of Leaves"},
	{"sp_a1_divinity_wall_blocks_wall",	"Wall Blocks Wall",  1,	"House of Leaves"},
	{"sp_a1_divinity_traversal",		"Traversal",		 1,	"House of Leaves"},
	{"sp_a1_divinity_funnel_tower",		"Funnel Tower",		 1,	"House of Leaves"},
	{"sp_a1_divinity_pink_plate",		"Pink Plate",		 1,	"House of Leaves"},
	{"sp_a1_divinity_core01",			"Core 01",			 1,	"House of Leaves"}
};

const std::vector<MapParams> DIVINITY_ADVANCED_MAPS =
{
	{"sp_adv_divinity_transit",	"Traversal Advanced",	1,	"Advanced Chambers"}
};

// Check to see which Divinity map is being played.
const MapParams* InDivinityCampaignMap(const bool advanced)
{
	if (advanced)
	{
		for (const auto& divinityAdvancedMap : DIVINITY_ADVANCED_MAPS)
		{
			if (FStrEq(CURMAPFILENAME, divinityAdvancedMap.mapFile))
				return &divinityAdvancedMap;
		}
	}
	else
	{
		for (const auto& divinityCampaignMap : DIVINITY_CAMPAIGN_MAPS)
		{
			if (FStrEq(CURMAPFILENAME, divinityCampaignMap.mapFile))
				return &divinityCampaignMap;
		}
	}
	
	return nullptr;
}