//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//
#pragma once

#include <string>

// Discord Embed Color Codes
#define EMBED_COLOR_PLAYER 61297 // Light Green
#define EMBED_COLOR_PLAYERDEATH 6881280 // Crimson Red
#define EMBED_COLOR_SERVER 4390995 // Dark Purple

class CDiscordIntegration {
public:
	static bool StartDiscordWebHooks();
	static void ShutdownDiscordWebHooks();
	static void SendWebHookEmbed(const std::string& title = "Unknown", const std::string& description = "*Insert Yapping Here*", int color = EMBED_COLOR_PLAYER, bool hasFooter = true);

	static bool StartDiscordRPC();
	static void ShutdownDiscordRPC();
	static void UpdateDiscordRPC();
	static bool DiscordRPCRunning();
};