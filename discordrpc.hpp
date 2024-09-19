//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//

#include "globals.hpp"

#include <thread>
#include <discord/discord.h>

// Discord Embed Color Codes
// !Change the color system to standard RGB format which translates to int HEX for the Discord Embed!
#define EMBEDCOLOR_PLAYER 61297 // Light Green
#define EMBEDCOLOR_PLAYERDEATH 6881280 // Crimson Red
#define EMBEDCOLOR_SERVER 4390995 // Dark Purple
//#define EMBEDCOLOR_PLAYER Color()

class CDiscordIntegration {
    public:
		void SendWebHookEmbed(std::string title = "Unknown", std::string description = "*Insert Yapping Here*", int color = EMBEDCOLOR_PLAYER, bool hasFooter = true);
};