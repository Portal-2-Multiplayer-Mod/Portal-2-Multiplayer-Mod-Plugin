//===========================================================================//
//
// Author: Orsell
// Purpose: Where ConVars and ConCommands are defined and used throughout the plugin.
// 
//===========================================================================//
#pragma once

#include "icvar.h"

//---------------------------------------------------------------------------------
// Core P2:MM ConVars | These shouldn't be modified manually. Hidden to prevent accidentally breaking something.
//---------------------------------------------------------------------------------
extern ConVar p2mm_loop;
extern ConVar p2mm_lastmap;
extern ConVar p2mm_splitscreen;

//---------------------------------------------------------------------------------
// Core P2:MM ConVars | These shouldn't be modified manually. Hidden to prevent accidentally breaking something.
//---------------------------------------------------------------------------------
extern ConVar p2mm_forbidclientcommands;
extern ConVar p2mm_deathicons;
extern ConVar p2mm_instantrespawn;

//---------------------------------------------------------------------------------
// Debug P2:MM ConVars | Self-explanatory.
//---------------------------------------------------------------------------------
extern ConVar p2mm_developer;
extern ConVar p2mm_spewgameeventinfo;