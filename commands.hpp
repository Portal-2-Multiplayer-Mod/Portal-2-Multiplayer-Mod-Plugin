//===========================================================================//
//
// Author: Orsell
// Purpose: Where ConVars and ConCommands are defined and used throughout the plugin.
// 
//===========================================================================//
#pragma once

#include "icvar.h"

#include <string>
#include <vector>

// Struct of player info storedS on the ban list.
typedef struct RemovePlayerInfo
{
	int			userID;	  // Player's unique ID on the server.
	std::string	username; // Player's username.
	std::string	guid;	  // Steam2 ID
} RemovePlayerInfo;
// The ban list itself, resets every time the game is started.
extern std::vector<RemovePlayerInfo> banList;
void RemovePlayerOperation(bool bBanning, int userid);
void RemovePlayerUI(int playerIndex, bool bBanning);

//---------------------------------------------------------------------------------
// Core P2:MM ConVars | These shouldn't be modified manually. Hidden to prevent accidentally breaking something.
//---------------------------------------------------------------------------------
extern ConVar p2mm_loop;
extern ConVar p2mm_lastmap;
extern ConVar p2mm_splitscreen;

//---------------------------------------------------------------------------------
// Core P2:MM ConVars | These shouldn't be modified manually. Hidden to prevent accidentally breaking something.
//---------------------------------------------------------------------------------
extern ConVar p2mm_forbid_clientcommands;
extern ConVar p2mm_deathicons;
extern ConVar p2mm_instantrespawn;

//---------------------------------------------------------------------------------
// Debug P2:MM ConVars | Self-explanatory.
//---------------------------------------------------------------------------------
extern ConVar p2mm_developer;
extern ConVar p2mm_spew_gameevent_info;