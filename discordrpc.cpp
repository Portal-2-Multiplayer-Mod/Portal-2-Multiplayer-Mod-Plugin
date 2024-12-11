//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//
#include "discordrpc.hpp"

#include "p2mm.hpp"

#include "discord-rpc/include/discord_register.h"
#include "discord-rpc/include/discord_rpc.h"
#include "curl/curl.h"

#include <iostream>
#include <thread>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Log Discord GameSDK logs to the console. This is mainly a developer mode only logging system and only the warning and error logs should be shown.
void DiscordLog(int level, bool dev, const char* pMsgFormat, ...)
{
	if (dev && !p2mm_developer.GetBool()) return; // Stop debug and info messages when p2mm_developer isn't enabled.

	// Take our log message and format any arguments it has into the message.
	va_list argptr;
	char szFormattedText[1024] = { 0 };
	va_start(argptr, pMsgFormat);
	V_vsnprintf(szFormattedText, sizeof(szFormattedText), pMsgFormat, argptr);
	va_end(argptr);

	// Add a header to the log message.
	char completeMsg[1024] = { 0 };
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
		Warning("(P2:MM DISCORD): DiscordLog level out of range, \"%i\". Defaulting to level 0.\n", level);
		ConColorMsg(P2MM_DISCORD_CONSOLE_COLOR_NORMAL, completeMsg);
		return;
	}
}

////-----------------------------------------------------------------------------
//// Discord Webhooks
////-----------------------------------------------------------------------------

void WebhookCheck(IConVar* var, const char* pOldValue, float flOldValue)
{
	// Make sure people know that the chat is being recorded if webhook is set
	if (((ConVar*)var)->GetBool())
	{
		FOR_ALL_PLAYERS(i)
		{
			CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
			if (pPlayer)
				UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, "This lobby has Discord Webhook Intergration enabled! All of your ingame messages may be sent to a Discord channel.");
		}
	}
}
ConVar p2mm_discord_webhooks("p2mm_discord_webhooks", "0", FCVAR_ARCHIVE | FCVAR_NOTIFY, "Enable or disable webhooks been the P2:MM Server and Discord.", true, 0, true, 1, WebhookCheck);
ConVar p2mm_discord_webhooks_url("p2mm_discord_webhooks_url", "", FCVAR_ARCHIVE | FCVAR_HIDDEN, "Channel webhook URL to send messages to. Should be set in launcher, not here.");
ConVar p2mm_discord_webhooks_defaultfooter("p2mm_discord_webhooks_defaultfooter", "1", FCVAR_ARCHIVE, "Enable or disable the default embed footer for webhooks.", true, 0, true, 1);
ConVar p2mm_discord_webhooks_customfooter("p2mm_discord_webhooks_customfooter", "", FCVAR_ARCHIVE, "Set a custom embed footer for webhook messages.");

// Parameters that are sent through to the Discord webhook
struct WebHookParams
{
	std::string title = "Unknown";
	std::string description = "*Insert Yapping Here*";
	int			color = EMBEDCOLOR_PLAYER;
	std::string footer = "";
};

// Generates a footer with the player count with max allowed client count and also the current map name
std::string DefaultFooter()
{
	// g_pGlobals doesn't exist yet at certain situations, so return a blank string.
	if (!g_pGlobals) return "";

	std::string curplayercount = std::to_string(CURPLAYERCOUNT());
	std::string maxplayercount = std::to_string(MAX_PLAYERS);

	std::string footer = std::string("Players: ") + curplayercount + "/" + maxplayercount + std::string(" || Current Map: ") + CURMAPFILENAME;

	return footer;
}

// Thread sending a curl request to the specified Discord WebHook
unsigned SendWebHook(void* webhookParams)
{
	if (p2mm_discord_webhooks.GetBool())
	{
		P2MMLog(0, true, "Webhook for \"p2mm_discord_webhooks_url\" has not been specified.");
		return 1;
	}

	CURL* curl = curl_easy_init();
	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!curl)
	{
		P2MMLog(1, false, "Failed to initalize curl request!");
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, p2mm_discord_webhooks_url.GetString());
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
		P2MMLog(1, false, "Failed to send curl request! Error Code: %i", curlCode);
	else
		P2MMLog(0, true, "Sent webhook curl request!");

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
	if (!p2mm_discord_webhooks.GetBool()) return;

	// Allocate memory for the parameters
	WebHookParams* webhookParams = new WebHookParams;
	webhookParams->title = title;	
	webhookParams->description = description;
	webhookParams->color = color;
	webhookParams->footer = "";

	// Set the footer to the default footer if enabled
	if (p2mm_discord_webhooks_defaultfooter.GetBool() && hasFooter)
	{
		webhookParams->footer = DefaultFooter();
	}
	else if (!p2mm_discord_webhooks_defaultfooter.GetBool() && hasFooter)
	{
		webhookParams->footer = std::string(p2mm_discord_webhooks_customfooter.GetString());
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
//// OLD API Source: https://github.com/discord/discord-rpc
//// OLD API Documentation: https://github.com/discord/discord-api-docs/tree/legacy-gamesdk/docs/rich_presence
////-----------------------------------------------------------------------------

void RPCState(IConVar* var, const char* pOldValue, float flOldValue)
{
	if (!g_P2MMServerPlugin.m_bPluginLoaded) return;
	ConVar* cvRPC = (ConVar*)var;
	if (cvRPC->GetBool() && !g_pDiscordIntegration->RPCRunning)
		g_pDiscordIntegration->StartDiscordRPC();
	if (!cvRPC->GetBool() && g_pDiscordIntegration->RPCRunning)
		g_pDiscordIntegration->ShutdownDiscordRPC();
}
ConVar p2mm_discord_rpc("p2mm_discord_rpc", "1", FCVAR_ARCHIVE, "Enable or disable Discord RPC with P2:MM.", true, 0, true, 1, RPCState);

static DiscordRichPresence RPC;
CDiscordIntegration::CDiscordIntegration()
{
	this->RPCRunning = false; // Flag bool for whether the RPC is running.

	RPC.state = "";
	RPC.details = "Starting up...";
	RPC.startTimestamp = time(0);
	RPC.endTimestamp = 0;
	RPC.largeImageKey = "p2mmlogo";
	RPC.largeImageText = "Portal 2";
	RPC.smallImageKey = "wave";
	RPC.smallImageText = "Welcome to P2:MM!";
	RPC.partyId = "";
	RPC.partySize = 0;
	RPC.partyMax = 0;
	RPC.matchSecret = "";
	RPC.joinSecret = "";
	RPC.spectateSecret = "";
	RPC.instance = 0;
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
	DiscordLog(1, false, "Discord: Disconnected (%d: %s)\n", errcode, message);
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

//---------------------------------------------------------------------------------
// Purpose: Starts up the Discord RPC for P2:MM.
//---------------------------------------------------------------------------------
bool CDiscordIntegration::StartDiscordRPC()
{
	DiscordLog(0, false, "Starting up Discord RPC!");

	DiscordLog(0, true, "Setting Discord event handlers...");
	DiscordEventHandlers* handlers = new DiscordEventHandlers;
	handlers->ready = HandleDiscordReady;
	handlers->disconnected = HandleDiscordDisconnected;
	handlers->errored = HandleDiscordError;
	handlers->joinGame = HandleDiscordJoin;
	handlers->spectateGame = HandleDiscordSpectate;
	handlers->joinRequest = HandleDiscordJoinRequest;

	DiscordLog(0, true, "Assosiating the plugin with the current Portal 2 branch game then initallizng RPC...");
	char appid[255];
	V_snprintf(appid, 255, "%d", engineServer->GetAppID());
	Discord_Initialize("1201562647880015954", handlers, 1, appid);

	if (g_P2MMServerPlugin.m_iCurGameIndex == 1)
	{
		RPC.largeImageKey = "p2mmmellogo";
		RPC.largeImageText = "Portal Stories: Mel";
	}
	UpdateDiscordRPC();

	DiscordLog(0, false, "Discord RPC activated!");
	this->RPCRunning = true;
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: Shuts down the Discord RPC for P2:MM.
//---------------------------------------------------------------------------------
void CDiscordIntegration::ShutdownDiscordRPC()
{
	DiscordLog(0, false, "Shutting down Discord RPC...");
	Discord_ClearPresence();
	Discord_Shutdown();
	this->RPCRunning = false;
	DiscordLog(0, false, "Shutdown Discord RPC!");
}

void CDiscordIntegration::UpdateDiscordRPC()
{
	DiscordLog(0, true, "Updating Discord RPC!");

	if (g_P2MMServerPlugin.m_bPluginUnloading)
	{
		RPC.state = "See you around!";
		RPC.details = "Shutting down...";
		RPC.smallImageKey = "wave";
		RPC.smallImageText = "Welcome to P2:MM!";
		RPC.partySize = 0;
		RPC.partyMax = 0;
		RPC.instance = 0;
	}

	if (!IsGameActive() || IsGameShutdown())
	{
		RPC.details = "Main Menu";
		RPC.smallImageKey = "wave";
		RPC.smallImageText = "Welcome to P2:MM!";
		RPC.partySize = 0;
		RPC.partyMax = 0;
		RPC.instance = 0;
	}

	if (IsGameActive() && (!g_P2MMServerPlugin.m_bPluginUnloading || !IsGameShutdown()))
	{
		MapParams* map = NULL;
		char details[128] = "Map: ";
		char smallImageKey[32] = { 0 };
		char smallImageText[128] = { 0 };
		switch (g_P2MMServerPlugin.m_iCurGameIndex)
		{
		case (0):
			if (FStrEq(CURMAPFILENAME, "mp_coop_community_hub"))
			{
				V_strcat(details, "Community Hub", 128);
				V_strcat(smallImageKey, "coop", 32);
				V_strcat(smallImageText, "Community Hub", 128);
			}
			else if (std::strstr(CURMAPFILENAME, "sp_"))
			{
				map = InP2CampaignMap();
				if (!map) break;

				V_strcat(details, map->mapname, 128);
				V_snprintf(smallImageKey, 32, "chapter%i", map->chapter);
				V_strcat(smallImageText, map->chaptername, 128);
			}
			else if (std::strstr(CURMAPFILENAME, "gelocity"))
			{
				map = InGelocityMap();
				if (!map) break;

				V_strcat(details, map->mapname, 128);
				V_strcat(smallImageKey, "race", 32);
				V_strcat(smallImageText, map->mapname, 128);
			}
			else if (std::strstr(CURMAPFILENAME, "workshop/"))
			{
				const char* lastForwardSlash = strrchr(CURMAPFILENAME, '/');
				if (!lastForwardSlash) break;
				V_strcpy(details, lastForwardSlash + 1);
				V_strcat(smallImageKey, "workshop", 32);
				V_strcat(smallImageText, "Workshop Map", 128);
			}
			else
			{
				map = InP2CampaignMap(true);
				if (!map) break;

				V_strcat(details, map->mapname, 128);
				V_strcat(smallImageKey, "coop", 32);
				V_strcat(smallImageText, map->chaptername, 128);
			}
			break;
		case (1):
			if (FStrEq(CURMAPFILENAME, "mp_coop_community_hub")) break;

			if (std::strstr(CURMAPFILENAME, "sp_"))
				map = InMelCampaignMap(true);
			else
				map = InMelCampaignMap();
			if (!map) break;

			V_strcat(details, map->mapname, 128);
			V_snprintf(smallImageKey, 32, "melchapter%i", map->chapter);
			V_strcat(smallImageText, map->chaptername, 128);
			break;
		default:
			break;
		}
		RPC.state = "Players: ";
		RPC.details = details;
		RPC.smallImageKey = smallImageKey;
		RPC.smallImageText = smallImageText;
		RPC.partySize = CURPLAYERCOUNT();
		RPC.partyMax = MAX_PLAYERS;
		RPC.instance = 1;
	}

	DiscordLog(0, true, "Discord RPC Debug Spew:");
	DiscordLog(0, true, "state: %s",RPC.state);
	DiscordLog(0, true, "details: %s", RPC.details);
	DiscordLog(0, true, "startTimestamp: %I64d", RPC.startTimestamp);
	DiscordLog(0, true, "endTimestamp: %I64d", RPC.endTimestamp);
	DiscordLog(0, true, "largeImageKey: %s", RPC.largeImageKey);
	DiscordLog(0, true, "largeImageText: %s", RPC.largeImageText);
	DiscordLog(0, true, "smallImageKey: %s", RPC.smallImageKey);
	DiscordLog(0, true, "smallImageText: %s", RPC.smallImageText);
	DiscordLog(0, true, "partyId: %s", RPC.partyId);
	DiscordLog(0, true, "partySize: %i", RPC.partySize);
	DiscordLog(0, true, "partyMax: %i", RPC.partyMax);
	DiscordLog(0, true, "matchSecret: %s", RPC.matchSecret);
	DiscordLog(0, true, "joinSecret: %s", RPC.joinSecret);
	DiscordLog(0, true, "spectateSecret: %s", RPC.spectateSecret);
	DiscordLog(0, true, "instance: %i", RPC.spectateSecret);
	Discord_UpdatePresence(&RPC);
}