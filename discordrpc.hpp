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
	CDiscordIntegration();

	void SendWebHookEmbed(std::string title = "Unknown", std::string description = "*Insert Yapping Here*", int color = EMBED_COLOR_PLAYER, bool hasFooter = true);
	bool StartDiscordRPC();
	void ShutdownDiscordRPC();
	static void UpdateDiscordRPC();

	bool rpcRunning;
};
extern CDiscordIntegration* g_pDiscordIntegration;