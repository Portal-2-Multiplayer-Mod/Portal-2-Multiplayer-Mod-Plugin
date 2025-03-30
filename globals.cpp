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
void Log(const LogLevel level, const bool dev, const char* pMsgFormat, ...)
{
	if (dev && !p2mm_developer.GetBool() && level != ERRORR) return; // Stop developer messages when p2mm_developer isn't enabled.

	// Take our log message and format any arguments it has into the message.
	va_list argPtr;
	char szFormattedText[1024] = { 0 };
	va_start(argPtr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argPtr);
	va_end(argPtr);

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
		Warning("(P2:MM PLUGIN): Log level set outside of INFO-ERROR, \"%i\". Defaulting to level INFO.\n", level);
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
// Purpose: Get player username by their entity index.
//---------------------------------------------------------------------------------
const char* GetPlayerName(const int playerIndex)
{
	if (playerIndex <= 0 || playerIndex > MAX_PLAYERS)
	{
		Log(WARNING, true, "Invalid index passed to GetPlayerName: %i! Returning ""!", playerIndex);
		return "";
	}

	player_info_t playerInfo;
	if (!engineServer->GetPlayerInfo(playerIndex, &playerInfo))
	{
		Log(WARNING, true, R"(Couldn't retrieve playerInfo of player index in GetPlayerName: %i! Returning ""!)", playerIndex);
		return "";
	}

	return playerInfo.name;
}

//---------------------------------------------------------------------------------
// Purpose: Get the account ID component of player SteamID by the player's entity index.
//---------------------------------------------------------------------------------
int GetSteamID(const int playerIndex)
{
	edict_t* pEdict = nullptr;
	if (playerIndex >= 0 && playerIndex < MAX_PLAYERS)
		pEdict = (g_pGlobals->pEdicts + playerIndex);

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
		Log(WARNING, false, R"(Could not find ConVar: "%s"! Returning ""!)", cvName);
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
		Log(WARNING, false, R"(Could not find ConVar: "%s"! Returning ""!)", cvName);
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
		Log(WARNING, false, "Could not set ConVar: \"%s\"!", cvName);
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
		Log(WARNING, false, R"(Could not set ConVar: "%s"!)", cvName);
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
		Log(WARNING, true, R"(Couldn't retrieve player info of player index "%i" in IsBot!)", playerIndex);
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
	{.mapFile= "workshop/596984281130013835/mp_coop_gelocity_1_v02", .mapName= "Gelocity 1", .chapter= 1, .chapterName= ""},
	{.mapFile= "workshop/594730048530814099/mp_coop_gelocity_2_v01", .mapName= "Gelocity 2", .chapter= 2, .chapterName= ""},
	{.mapFile= "workshop/613885499245125173/mp_coop_gelocity_3_v02", .mapName= "Gelocity 3", .chapter= 3, .chapterName= ""}
};

// Check if the host is in a Gelocity map.
// Returns which Gelocity workshop is being played.
// Returns 0 if not in a Gelocity map.
const MapParams* InGelocityMap()
{
	for (const auto& gelocityMap : gelocityMaps)
	{
		if (FStrEq(CUR_MAPFILE_NAME, gelocityMap.mapFile))
			return &gelocityMap;
	}

	return nullptr;
}

//---------------------------------------------------------------------------------
// PORTAL 2 SINGLE PLAYER CAMPAIGN
//---------------------------------------------------------------------------------
const std::vector<MapParams> SP_CAMPAIGN_MAPS =
{
	{.mapFile= "sp_a1_intro1",                 .mapName= "Container Ride",       .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_intro2",                 .mapName= "Portal Carousel",      .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_intro3",                 .mapName= "Portal Gun",           .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_intro4",                 .mapName= "Smooth Jazz",          .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_intro5",                 .mapName= "Cube Momentum",        .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_intro6",                 .mapName= "Future Starter",       .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_intro7",                 .mapName= "Secret Panel",         .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a1_wakeup",                 .mapName= "Wakeup",				.chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a2_intro",                  .mapName= "Incinerator",          .chapter= 1, .chapterName= "The Courtesy Call"},
	{.mapFile= "sp_a2_laser_intro",            .mapName= "Laser Intro",          .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_laser_stairs",           .mapName= "Laser Stairs",         .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_dual_lasers",            .mapName= "Dual Lasers",          .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_laser_over_goo",         .mapName= "Laser Over Goo",       .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_catapult_intro",         .mapName= "Catapult Intro",       .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_trust_fling",            .mapName= "Trust Fling",          .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_pit_flings",             .mapName= "Pit Flings",           .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_fizzler_intro",          .mapName= "Fizzler Intro",        .chapter= 2, .chapterName= "The Cold Boot"},
	{.mapFile= "sp_a2_sphere_peek",            .mapName= "Ceiling Catapult",     .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_ricochet",               .mapName= "Ricochet",             .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_bridge_intro",           .mapName= "Bridge Intro",         .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_bridge_the_gap",         .mapName= "Bridge The Gap",       .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_turret_intro",           .mapName= "Turret Intro",         .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_laser_relays",           .mapName= "Laser Relays",         .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_turret_blocker",         .mapName= "Turret Blocker",       .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_laser_vs_turret",        .mapName= "Laser vs Turret",      .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_pull_the_rug",           .mapName= "Pull the Rug",         .chapter= 3, .chapterName= "The Return"},
	{.mapFile= "sp_a2_column_blocker",         .mapName= "Column Blocker",       .chapter= 4, .chapterName= "The Surprise"},
	{.mapFile= "sp_a2_laser_chaining",         .mapName= "Laser Chaining",       .chapter= 4, .chapterName= "The Surprise"},
	{.mapFile= "sp_a2_triple_laser",           .mapName= "Triple Laser",         .chapter= 4, .chapterName= "The Surprise"},
	{.mapFile= "sp_a2_bts1",                   .mapName= "Jailbreak",            .chapter= 4, .chapterName= "The Surprise"},
	{.mapFile= "sp_a2_bts2",                   .mapName= "Escape",				.chapter= 4, .chapterName= "The Surprise"},
	{.mapFile= "sp_a2_bts3",                   .mapName= "Turret Factory",       .chapter= 5, .chapterName= "The Escape"},
	{.mapFile= "sp_a2_bts4",                   .mapName= "Turret Sabotage",      .chapter= 5, .chapterName= "The Escape"},
	{.mapFile= "sp_a2_bts5",                   .mapName= "Neurotoxin Sabotage",  .chapter= 5, .chapterName= "The Escape"},
	{.mapFile= "sp_a2_bts6",                   .mapName= "Tube Ride",            .chapter= 5, .chapterName= "The Escape"},
	{.mapFile= "sp_a2_core",                   .mapName= "Core",					.chapter= 5, .chapterName= "The Escape"},
	{.mapFile= "sp_a3_00",                     .mapName= "Long Fall",            .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_01",                     .mapName= "Underground",          .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_03",                     .mapName= "Cave Johnson",         .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_jump_intro",             .mapName= "Repulsion Intro",      .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_bomb_flings",            .mapName= "Bomb Flings",          .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_crazy_box",              .mapName= "Crazy Box",            .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_transition01",           .mapName= "PotatOS",              .chapter= 6, .chapterName= "The Fall"},
	{.mapFile= "sp_a3_speed_ramp",             .mapName= "Propulsion Intro",     .chapter= 7, .chapterName= "The Reunion"},
	{.mapFile= "sp_a3_speed_flings",           .mapName= "Propulsion Flings",    .chapter= 7, .chapterName= "The Reunion"},
	{.mapFile= "sp_a3_portal_intro",           .mapName= "Conversion Intro",     .chapter= 7, .chapterName= "The Reunion"},
	{.mapFile= "sp_a3_end",                    .mapName= "Three Gels",           .chapter= 7, .chapterName= "The Reunion"},
	{.mapFile= "sp_a4_intro",                  .mapName= "Test",					.chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_tb_intro",               .mapName= "Funnel Intro",         .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_tb_trust_drop",          .mapName= "Ceiling Button",       .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_tb_wall_button",         .mapName= "Wall Button",          .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_tb_polarity",            .mapName= "Polarity",             .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_tb_catch",               .mapName= "Funnel Catch",         .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_stop_the_box",           .mapName= "Stop the Box",         .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_laser_catapult",         .mapName= "Laser Catapult",       .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_laser_platform",         .mapName= "Laser Platform",       .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_speed_tb_catch",         .mapName= "Propulsion Catch",     .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_jump_polarity",          .mapName= "Repulsion Polarity",   .chapter= 8, .chapterName= "The Itch"},
	{.mapFile= "sp_a4_finale1",                .mapName= "Finale 1",             .chapter= 9, .chapterName= "THE Part Where He Kills You"},
	{.mapFile= "sp_a4_finale2",                .mapName= "Finale 2",             .chapter= 9, .chapterName= "THE Part Where He Kills You"},
	{.mapFile= "sp_a4_finale3",                .mapName= "Finale 3",             .chapter= 9, .chapterName= "THE Part Where He Kills You"},
	{.mapFile= "sp_a4_finale4",                .mapName= "Finale 4",             .chapter= 9, .chapterName= "THE Part Where He Kills You"},
	{.mapFile= "sp_a5_credits",				   .mapName= "Credits",			 	.chapter= 10, .chapterName= "The End"}
};

//---------------------------------------------------------------------------------
// PORTAL 2 COOPERATIVE CAMPAIGN
//---------------------------------------------------------------------------------
const std::vector<MapParams> MP_CAMPAIGN_MAPS =
{
	{.mapFile= "mp_coop_start",                .mapName= "Calibration",          .chapter= 0, .chapterName= "Introduction"},
	{.mapFile= "mp_coop_lobby_2",              .mapName= "Cooperative Lobby",    .chapter= 0, .chapterName= "Introduction"},
	{.mapFile= "mp_coop_lobby_3",              .mapName= "Cooperative Lobby",    .chapter= 0, .chapterName= "Introduction"},
	{.mapFile= "mp_coop_community_hub",        .mapName= "Community Hub",		.chapter= 1, .chapterName= "Community Hub"},
	{.mapFile= "mp_coop_doors",                .mapName= "Doors",				.chapter= 1, .chapterName= "Team Building"},
	{.mapFile= "mp_coop_race_2",               .mapName= "Buttons",              .chapter= 1, .chapterName= "Team Building"},
	{.mapFile= "mp_coop_laser_2",              .mapName= "Lasers",				.chapter= 1, .chapterName= "Team Building"},
	{.mapFile= "mp_coop_rat_maze",             .mapName= "Rat Maze",             .chapter= 1, .chapterName= "Team Building"},
	{.mapFile= "mp_coop_laser_crusher",        .mapName= "Laser Crusher",        .chapter= 1, .chapterName= "Team Building"},
	{.mapFile= "mp_coop_teambts",              .mapName= "Behind the Scenes",    .chapter= 1, .chapterName= "Team Building"},
	{.mapFile= "mp_coop_fling_3",              .mapName= "Flings",				.chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_infinifling_train",    .mapName= "Infinifling",          .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_come_along",           .mapName= "Team Retrieval",       .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_fling_1",              .mapName= "Vertical Flings",      .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_catapult_1",           .mapName= "Catapults",            .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_multifling_1",         .mapName= "Multifling",           .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_fling_crushers",       .mapName= "Fling Crushers",       .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_fan",                  .mapName= "Industrial Fan",       .chapter= 2, .chapterName= "Mass And Velocity"},
	{.mapFile= "mp_coop_wall_intro",           .mapName= "Cooperative Bridges",  .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_wall_2",               .mapName= "Bridge Swap",          .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_catapult_wall_intro",  .mapName= "Fling Block",          .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_wall_block",           .mapName= "Catapult Block",       .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_catapult_2",           .mapName= "Bridge Fling",         .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_turret_walls",         .mapName= "Turret Walls",         .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_turret_ball",          .mapName= "Turret Assassin",      .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_wall_5",               .mapName= "Bridge Testing",       .chapter= 3, .chapterName= "Hard-Light Surfaces"},
	{.mapFile= "mp_coop_tbeam_redirect",       .mapName= "Cooperative Funnels",  .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_drill",          .mapName= "Funnel Drill",         .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_catch_grind_1",  .mapName= "Funnel Catch Coop",    .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_laser_1",        .mapName= "Funnel Laser",         .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_polarity",       .mapName= "Cooperative Polarity", .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_polarity2",      .mapName= "Funnel Hop",           .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_polarity3",      .mapName= "Advanced Polarity",    .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_maze",           .mapName= "Funnel Maze",          .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_tbeam_end",            .mapName= "Turret Warehouse",     .chapter= 4, .chapterName= "Excursion Funnels"},
	{.mapFile= "mp_coop_paint_come_along",     .mapName= "Repulsion Jumps",      .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_redirect",       .mapName= "Double Bounce",        .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_bridge",         .mapName= "Bridge Repulsion",     .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_walljumps",      .mapName= "Wall Repulsion",       .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_speed_fling",    .mapName= "Propulsion Crushers",  .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_red_racer",      .mapName= "Turret Ninja",         .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_speed_catch",    .mapName= "Propulsion Retrieval", .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_paint_longjump_intro", .mapName= "Vault Entrance",       .chapter= 5, .chapterName= "Mobility Gels"},
	{.mapFile= "mp_coop_credits",			   .mapName= "Credits",			  	.chapter= 5, .chapterName= "The End! :D"},
	{.mapFile= "mp_coop_separation_1",         .mapName= "Separation",           .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_tripleaxis",           .mapName= "Triple Axis",          .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_catapult_catch",       .mapName= "Catapult Catch",       .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_2paints_1bridge",      .mapName= "Bridge Gels",          .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_paint_conversion",     .mapName= "Maintenance",          .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_bridge_catch",         .mapName= "Bridge Catch",         .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_laser_tbeam",          .mapName= "Double Lift",          .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_paint_rat_maze",       .mapName= "Gel Maze",             .chapter= 6, .chapterName= "Art Therapy"},
	{.mapFile= "mp_coop_paint_crazy_box",      .mapName= "Crazier Box",          .chapter= 6, .chapterName= "Art Therapy"}
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
			if (FStrEq(CUR_MAPFILE_NAME, mpCampaignMap.mapFile))
				return &mpCampaignMap;
		}
	}
	else
	{
		for (const auto& spCampaignMap : SP_CAMPAIGN_MAPS)
		{
			if (FStrEq(CUR_MAPFILE_NAME, spCampaignMap.mapFile))
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
	{.mapFile= "st_a1_tramride",      .mapName= "Tram Ride",			.chapter= 1, .chapterName= "1952"},
	{.mapFile= "st_a1_mel_intro",     .mapName= "Mel Intro",			.chapter= 1, .chapterName= "1952"},
	{.mapFile= "st_a1_lift",          .mapName= "Lift",				.chapter= 1, .chapterName= "1952"},
	{.mapFile= "st_a1_garden",        .mapName= "Garden",			.chapter= 1, .chapterName= "1952"},
	{.mapFile= "st_a2_garden_de",     .mapName= "Destroyed Garden",	.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "st_a2_underbounce",   .mapName= "Underbounce",		.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "st_a2_once_upon",     .mapName= "Once Upon",			.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "st_a2_past_power",    .mapName= "Past Power",		.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "st_a2_ramp",          .mapName= "Ramp",				.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "st_a2_firestorm",     .mapName= "Firestorm",			.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "st_a3_junkyard",      .mapName= "Junkyard",			.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "st_a3_concepts",      .mapName= "Concepts",			.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "st_a3_paint_fling",   .mapName= "Paint Fling",		.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "st_a3_faith_plate",   .mapName= "Faith Plate",		.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "st_a3_transition",    .mapName= "Transition",		.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "st_a4_overgrown",     .mapName= "Overgrown",			.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "st_a4_tb_over_goo",   .mapName= "Funnel Over Goo",	.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "st_a4_two_of_a_kind", .mapName= "Two of a Kind",		.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "st_a4_destroyed",     .mapName= "Destroyed",			.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "st_a4_factory",       .mapName= "Factory",			.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "st_a4_core_access",   .mapName= "Core Access",		.chapter= 5, .chapterName= "INTRUSION"},
	{.mapFile= "st_a4_finale",        .mapName= "Finale",			.chapter= 5, .chapterName= "INTRUSION"},
	{.mapFile= "st_a5_credits",		 .mapName= "Credits",			.chapter= 6, .chapterName= "The End"}
};

const std::vector<MapParams> MEL_ADVANCED_CAMPAIGN_MAPS =
{
	{.mapFile= "sp_a1_tramride",      .mapName= "Tram Ride (Advanced)",			.chapter= 1, .chapterName= "1952"},
	{.mapFile= "sp_a1_mel_intro",     .mapName= "Mel Intro (Advanced)",			.chapter= 1, .chapterName= "1952"},
	{.mapFile= "sp_a1_lift",          .mapName= "Lift (Advanced)",				.chapter= 1, .chapterName= "1952"},
	{.mapFile= "sp_a1_garden",        .mapName= "Garden (Advanced)",				.chapter= 1, .chapterName= "1952"},
	{.mapFile= "sp_a2_garden_de",     .mapName= "Destroyed Garden (Advanced)",	.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "sp_a2_underbounce",   .mapName= "Underbounce (Advanced)",		.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "sp_a2_once_upon",     .mapName= "Once Upon (Advanced)",			.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "sp_a2_past_power",    .mapName= "Past Power (Advanced)",			.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "sp_a2_ramp",          .mapName= "Ramp (Advanced)",				.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "sp_a2_firestorm",     .mapName= "Firestorm (Advanced)",			.chapter= 2, .chapterName= "Extended Relaxation"},
	{.mapFile= "sp_a3_junkyard",      .mapName= "Junkyard (Advanced)",			.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "sp_a3_concepts",      .mapName= "Concepts (Advanced)",			.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "sp_a3_paint_fling",   .mapName= "Paint Fling (Advanced)",		.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "sp_a3_faith_plate",   .mapName= "Faith Plate (Advanced)",		.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "sp_a3_transition",    .mapName= "Transition (Advanced)",			.chapter= 3, .chapterName= "The Ascent"},
	{.mapFile= "sp_a4_overgrown",     .mapName= "Overgrown (Advanced)",			.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "sp_a4_tb_over_goo",   .mapName= "Funnel Over Goo (Advanced)",	.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "sp_a4_two_of_a_kind", .mapName= "Two of a Kind (Advanced)",		.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "sp_a4_destroyed",     .mapName= "Destroyed (Advanced)",			.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "sp_a4_factory",       .mapName= "Factory (Advanced)",			.chapter= 4, .chapterName= "Organic Complications"},
	{.mapFile= "sp_a4_core_access",   .mapName= "Core Access (Advanced)",		.chapter= 5, .chapterName= "INTRUSION"},
	{.mapFile= "sp_a4_finale",        .mapName= "Finale (Advanced)",				.chapter= 5, .chapterName= "INTRUSION"},
	{.mapFile= "sp_a5_credits",		  .mapName= "Credits (Advanced)",			.chapter= 6, .chapterName= "The End"}
};

// Check to see which Mel map is being played.
const MapParams* InMelCampaignMap(const bool advanced)
{
	if (advanced)
	{
		for (const auto& melAdvancedCampaignMap : MEL_ADVANCED_CAMPAIGN_MAPS)
		{
			if (FStrEq(CUR_MAPFILE_NAME, melAdvancedCampaignMap.mapFile))
				return &melAdvancedCampaignMap;
		}
	}
	else
	{
		for (const auto& melStoryCampaignMap : MEL_STORY_CAMPAIGN_MAPS)
		{
			if (FStrEq(CUR_MAPFILE_NAME, melStoryCampaignMap.mapFile))
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
	{.mapFile= "gg_intro_wakeup",			.mapName= "Wake Up/Intro",					.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_blue_only",				.mapName= "Blue Only I",					.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_blue_only_2",				.mapName= "Blue Only II",					.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_blue_only_3",				.mapName= "Blue Only III",					.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_blue_only_2_pt2",			.mapName= "Blue Only IV",					.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_a1_intro4",				.mapName= "Smooth Jazz",					.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_blue_upplatform",			.mapName= "Portals",						.chapter= 1, .chapterName= "Aperture Tag"},
	{.mapFile= "gg_red_only",				.mapName= "Speed Gel Upgrade",				.chapter= 2, .chapterName= "Speed Gel Upgrade"},
	{.mapFile= "gg_red_surf",				.mapName= "Surf",							.chapter= 2, 	.chapterName= "Speed Gel Upgrade"},
	{.mapFile= "gg_all_intro",				.mapName= "All Intro",         				.chapter= 2, .chapterName= "Speed Gel Upgrade"},
	{.mapFile= "gg_all_rotating_wall",		.mapName= "Rotating Wall",     				.chapter= 2, .chapterName= "Speed Gel Upgrade"},
	{.mapFile= "gg_all_fizzler",				.mapName= "Fizzlers",          				.chapter= 2, .chapterName= "Speed Gel Upgrade"},
	{.mapFile= "gg_all_intro_2",				.mapName= "All Intro 2",       				.chapter= 2, .chapterName= "Speed Gel Upgrade"},
	{.mapFile= "gg_a2_column_blocker",		.mapName= "Column Blocker",					.chapter= 3, .chapterName= "No More Recycling Tests"},
	{.mapFile= "gg_all_puzzle2",				.mapName= "Portals 2",						.chapter= 3, .chapterName= "No More Recycling Tests"},
	{.mapFile= "gg_all2_puzzle1",			.mapName= "Future Starter",					.chapter= 3, .chapterName= "No More Recycling Tests"},
	{.mapFile= "gg_all_puzzle1",				.mapName= "Final Qualification",			.chapter= 3, .chapterName= "No More Recycling Tests"},
	{.mapFile= "gg_all2_escape1",			.mapName= "ALSSER/Escape",					.chapter= 3, .chapterName= "No More Recycling Tests"},
	{.mapFile= "gg_stage_reveal",			.mapName= "Reveal",							.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_bridgebounce_2",	.mapName= "Bridge Bounce",    				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_redfirst",			.mapName= "Red First",        				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_laserrelay",	 	.mapName= "Laser Relay",      				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_beamscotty",	 	.mapName= "Citranium Desert", 				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_bridgebounce",	 	.mapName= "Bridge Bounce 2",  				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_roofbounce",	 	.mapName= "Roof Bounce",      				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_pickbounce",	 	.mapName= "Pick Bounce",      				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_stage_theend",		 	.mapName= "The End",          				.chapter= 4, .chapterName= "The Stage"},
	{.mapFile= "gg_credit_video", 			.mapName= "Game Credits",                   .chapter= 5, .chapterName= "Extras"},
	{.mapFile= "gg_trailer_map",  			.mapName= "Trailer Map",                    .chapter= 5, .chapterName= "Extras"},
	{.mapFile= "gg_tag_remix",				.mapName= "TAG: The Power of Paint Remake", .chapter= 5, .chapterName= "Extras"}
};

// Check to see which Aperture Tag map is being played.
const MapParams* InApertureTagCampaignMap()
{
	for (const auto& apertureTagCampaignMap : APERTURE_TAG_CAMPAIGN_MAPS)
	{
		if (FStrEq(CUR_MAPFILE_NAME, apertureTagCampaignMap.mapFile))
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
// 			if (FStrEq(CUR_MAPFILE_NAME, portalReloadedSPCampaignMap.mapFile))
// 				return &portalReloadedSPCampaignMap;
// 		}
// 	}
// 	else
// 	{
// 		for (const auto& portalReloadedMPCampaignMap : PORTAL_RELOADED_MP_CAMPAIGN_MAPS)
// 		{
// 			if (FStrEq(CUR_MAPFILE_NAME, portalReloadedMPCampaignMap.mapFile))
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
	{.mapFile= "sp_a1_divinity_intro",			.mapName= "Intro",				.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_bts1",			.mapName= "BTS 1",				.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_field_intro",		.mapName= "Field Intro",		.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_wall_blocks_wall",.mapName= "Wall Blocks Wall",	.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_traversal",		.mapName= "Traversal",			.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_funnel_tower",	.mapName= "Funnel Tower",		.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_pink_plate",		.mapName= "Pink Plate",			.chapter= 1, .chapterName= "House of Leaves"},
	{.mapFile= "sp_a1_divinity_core01",			.mapName= "Core 01",			.chapter= 1, .chapterName= "House of Leaves"}
};

const std::vector<MapParams> DIVINITY_ADVANCED_MAPS =
{
	{.mapFile= "sp_adv_divinity_transit", .mapName= "Traversal Advanced", .chapter= 1, .chapterName= "Advanced Chambers"}
};

// Check to see which Divinity map is being played.
const MapParams* InDivinityCampaignMap(const bool advanced)
{
	if (advanced)
	{
		for (const auto& divinityAdvancedMap : DIVINITY_ADVANCED_MAPS)
		{
			if (FStrEq(CUR_MAPFILE_NAME, divinityAdvancedMap.mapFile))
				return &divinityAdvancedMap;
		}
	}
	else
	{
		for (const auto& divinityCampaignMap : DIVINITY_CAMPAIGN_MAPS)
		{
			if (FStrEq(CUR_MAPFILE_NAME, divinityCampaignMap.mapFile))
				return &divinityCampaignMap;
		}
	}
	
	return nullptr;
}