//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//

#include "discordrpc.hpp"

#include <iostream>
#include <string>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Discord integration ConVars
ConVar p2mm_discord_webhook("p2mm_discord_webhook", "", FCVAR_HIDDEN, "Channel webhook URL to send messages to. Should be set in launcher, not here.", true, 0, true, 1, WebhookCheck);
ConVar p2mm_discord_webhook_defaultfooter("p2mm_discord_webhook_defaultfooter", "1", FCVAR_NONE, "Enable or disable the default embed footer for webhooks.", true, 0, true, 1);
ConVar p2mm_discord_webhook_customfooter("p2mm_discord_webhook_customfooter", "", FCVAR_NONE, "Set a custom embed footer for webhook messages.");
ConVar p2mm_discord_rpc("p2mm_discord_rpc", "1", FCVAR_NONE, "Enable or disable Discord RPC with P2:MM.");
ConVar p2mm_discord_rpc_appid("p2mm_discord_rpc_appid", "1201562647880015954", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Application ID used for Discord RPC with P2:MM.");

int timestamp = time(0);

void WebhookCheck(IConVar* var, const char* pOldValue, float flOldValue)
{
	// Make sure people know that the chat is being recorded if webhook is set
	if (strlen(p2mm_discord_webhook.GetString()) > 0)
	{
		FOR_ALL_PLAYERS(i)
		{
			player_info_t playerinfo;
			if (engineServer->GetPlayerInfo(i, &playerinfo))
			{
				UTIL_ClientPrint(UTIL_PlayerByIndex(i), HUD_PRINTTALK, "This lobby has Discord Webhook Intergration enabled. All of your ingame messages may be sent to a Discord channel.");
			}
		}
		P2MMLog(0, true, "Webhook warning called");
	}
}

// Log Discord GameSDK logs to the console. This is mainly a developer mode only logging system and only the warning and error logs should be shown.
void DiscordLog(int level, bool dev, const char* pMsgFormat, ...)
{
	if (!p2mm_developer.GetBool()) { return; } // Stop debug and info messages when p2mm_developer isn't enabled.

	// Take our log message and format any arguments it has into the message.
	va_list argptr;
	char szFormattedText[1024];
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	// Add a header to the log message.
	char completeMsg[1024];
	V_snprintf(completeMsg, sizeof(completeMsg), "(P2:MM DISCORD): %s\n", szFormattedText);

	switch (level)
	{
	case 0:
		ConColorMsg(P2MM_DISCORD_CONSOLE_COLOR_NORMAL, completeMsg);
		return;
	case 1:
		ConColorMsg(P2MM_DISCORD_CONSOLE_COLOR_WARNING, completeMsg);
		return;
	default:
		Warning("(P2:MM DISCORD): DiscordLog level out of range, \"%i\". Defaulting to discord::LogLevel::Info.\n", level);
		ConColorMsg(P2MM_DISCORD_CONSOLE_COLOR_NORMAL, completeMsg);
		return;
	}
}

////-----------------------------------------------------------------------------
//// Discord Webhooks
////-----------------------------------------------------------------------------

// Generates a footer with the player count with max allowed client count and also the current map name
std::string DefaultFooter()
{
	// g_pGlobals doesn't exist yet at certain situations, so return a blank string.
	if (!g_pGlobals) { return ""; }

	std::string curplayercount = std::to_string(CURPLAYERCOUNT());
	std::string maxplayercount = std::to_string(g_pGlobals->maxClients);

	std::string footer = std::string("Players: ") + curplayercount + "/" + maxplayercount + std::string(" || Current Map: ") + CURMAPNAME;

	return footer;
}

// Parameters that are sent through to the Discord webhook
struct WebHookParams
{
	std::string title;
	std::string description;
	int			color;
	std::string footer;
};

// Thread sending a curl request to the specified Discord WebHook
unsigned SendWebHook(void* webhookParams)
{
	if (strlen(p2mm_discord_webhook.GetString()) <= 0)
	{
		P2MMLog(0, true, "Webhook for 'p2mm_discord_webhook' has not been specified.");
		return 1;
	}

	CURL* curl = curl_easy_init();
	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!curl)
	{
		P2MMLog(1, false, "Failed to initalize curl request!");
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, p2mm_discord_webhook.GetString());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);

	// Create the JSON payload
	WebHookParams* params = (WebHookParams*)webhookParams;

	std::string jsonPayload = std::string(
		"{ \"content\": null, \"embeds\" : [ {\"title\": \"" +
		params->title + "\", \"description\" : \"" +
		params->description + "\", \"color\" : " +
		std::to_string(params->color) + ", \"footer\": { \"text\": \"" +
		params->footer + "\" }}], \"attachments\": [] }"
	);

	P2MMLog(0, true, std::string("jsonPayload: " + jsonPayload).c_str());

	// Set the POST data
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload);

	// Set the Content-Type header
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	CURLcode curlCode = curl_easy_perform(curl);

	// Perform the request and check for errors
	if (curlCode != CURLE_OK)
	{
		P2MMLog(1, false, "Failed to send curl request! Error Code: %i", curlCode);
	}
	else {
		P2MMLog(0, true, "Sent webhook curl request!");
	}

	// Cleanup curl request
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	curl_global_cleanup();

	// Free the message parameters from memory
	delete webhookParams;

	return 0;
}

// Send a embed message to Discord via a webhook
void CDiscordIntegration::SendWebHookEmbed(std::string title, std::string description, int color, bool hasFooter)
{
	// Allocate memory for the parameters
	WebHookParams* webhookParams = new WebHookParams;
	webhookParams->title = title;	
	webhookParams->description = description;
	webhookParams->color = color;
	webhookParams->footer = "";

	// Set the footer to the default footer if enabled
	if (p2mm_discord_webhook_defaultfooter.GetBool() && hasFooter)
	{
		webhookParams->footer = DefaultFooter();
	}
	else if (!p2mm_discord_webhook_defaultfooter.GetBool() && hasFooter)
	{
		webhookParams->footer = std::string(p2mm_discord_webhook_customfooter.GetString());
	}

	P2MMLog(0, true, "Embed webhookParams:");
	P2MMLog(0, true, std::string("title: " + title).c_str());
	P2MMLog(0, true, std::string("description: " + description).c_str());
	P2MMLog(0, true, std::string("color: " + std::to_string(color)).c_str());
	P2MMLog(0, true, std::string("footer: " + webhookParams->footer).c_str());

	// Send the curl request in a seperate thread
	CreateSimpleThread(SendWebHook, webhookParams);
}

////-----------------------------------------------------------------------------
//// Discord Rich Presence
//// OLD API Documentation: https://github.com/discord/discord-api-docs/tree/legacy-gamesdk/docs/rich_presence
////-----------------------------------------------------------------------------

void UpdateDiscordRPC(
	const char* state = "", /* max 128 bytes */
	const char* details = "", /* max 128 bytes */
	int64_t startTimestamp = time(0),
	int64_t endTimestamp = time(0),
	const char* largeImageKey = "p2mmlogo", /* max 32 bytes */
	const char* largeImageText = "P2:MM", /* max 128 bytes */
	const char* smallImageKey = "", /* max 32 bytes */
	const char* smallImageText = "", /* max 128 bytes */
	const char* partyId = "", /* max 128 bytes */
	int partySize = 0,
	int partyMax = 33,
	const char* matchSecret = "", /* max 128 bytes */
	const char* joinSecret = "", /* max 128 bytes */
	const char* spectateSecret = "", /* max 128 bytes */
	int8_t instance = 0
)
{
	DiscordRichPresence discordPresence;
	discordPresence.state = state;
	discordPresence.details = details;
	discordPresence.startTimestamp = startTimestamp;
	discordPresence.endTimestamp = time(0) + 5 * 60;
	discordPresence.largeImageKey = "canary-large";
	discordPresence.smallImageKey = "ptb-small";
	discordPresence.partyId = "party1234";
	discordPresence.partySize = 1;
	discordPresence.partyMax = 6;
	//discordPresence.partyPrivacy = DISCORD_PARTY_PUBLIC;
	discordPresence.matchSecret = "xyzzy";
	discordPresence.joinSecret = "join";
	discordPresence.spectateSecret = "look";
	discordPresence.instance = 0;
}

static void HandleDiscordReady(const DiscordUser* connectedUser)
{
	DiscordLog(0, false, "Discord: Connected to user %s#%s - %s\n",
		connectedUser->username,
		connectedUser->discriminator,
		connectedUser->userId);
}

static void HandleDiscordDisconnected(int errcode, const char* message)
{
	DiscordLog(0, false, "Discord: Disconnected (%d: %s)\n", errcode, message);
}

static void HandleDiscordError(int errcode, const char* message)
{
	DiscordLog(1, false, "Discord: Error (%d: %s)\n", errcode, message);
}

static void HandleDiscordJoin(const char* secret)
{
	// Not implemented
}

static void HandleDiscordSpectate(const char* secret)
{
	// Not implemented
}

static void HandleDiscordJoinRequest(const DiscordUser* request)
{
	// Not implemented
}

bool CDiscordIntegration::StartDiscordRPC()
{
	// Discord RPC
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));

	handlers.ready = HandleDiscordReady;
	handlers.disconnected = HandleDiscordDisconnected;
	handlers.errored = HandleDiscordError;
	handlers.joinGame = HandleDiscordJoin;
	handlers.spectateGame = HandleDiscordSpectate;
	handlers.joinRequest = HandleDiscordJoinRequest;

	char appid[255];
	sprintf(appid, "%d", engineServer->GetAppID());
	Discord_Initialize(p2mm_discord_rpc_appid.GetString(), &handlers, 1, appid);
	
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	discordPresence.details = "Starting up...";
	discordPresence.startTimestamp = timestamp;
	discordPresence.largeImageKey = "p2mmlogo";
	Discord_UpdatePresence(&discordPresence);
	
	return true;
}

void CDiscordIntegration::ShutdownDiscordRPC()
{
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	discordPresence.details = "Shutting down...";
	discordPresence.startTimestamp = timestamp;
	discordPresence.largeImageKey = "p2mmlogo";
	Discord_UpdatePresence(&discordPresence);
}