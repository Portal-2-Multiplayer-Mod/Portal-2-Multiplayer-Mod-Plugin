//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//

#include "globals.hpp"

#include <thread>

#include "discord-rpc/include/discord_register.h"
#include "discord-rpc/include/discord_rpc.h"
#include "curl/curl.h"

// Discord Embed Color Codes
// !Change the color system to standard RGB format which translates to int HEX for the Discord Embed!
#define EMBEDCOLOR_PLAYER 61297 // Light Green
#define EMBEDCOLOR_PLAYERDEATH 6881280 // Crimson Red
#define EMBEDCOLOR_SERVER 4390995 // Dark Purple

// Even programs have to sleep sometimes
#if _WIN32
#define MIMIMIMI(ms) Sleep(ms); // MIMIMIMI *snore* MIMIMIMI
#else
#define MIMIMIMI(ms) usleep(ms); // MIMIMIMI *snores in Linux* MIMIMIMI
#endif

extern ConVar p2mm_discord_rpc;
extern ConVar p2mm_discord_webhook;

class CDiscordIntegration {
public:
	bool RPCRunning;
	DiscordRichPresence* RPC;

	CDiscordIntegration();
	~CDiscordIntegration();

	void SendWebHookEmbed(std::string title = "Unknown", std::string description = "*Insert Yapping Here*", int color = EMBEDCOLOR_PLAYER, bool hasFooter = true);
	bool StartDiscordRPC();
	void ShutdownDiscordRPC();
};

extern CDiscordIntegration* g_pDiscordIntegration;