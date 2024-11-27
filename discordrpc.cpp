//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//

#include "discordrpc.hpp"
#include "p2mm.hpp"

#include <iostream>
#include <string>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Discord integration ConVars
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
ConVar p2mm_discord_webhook("p2mm_discord_webhook", "0", FCVAR_ARCHIVE | FCVAR_NOTIFY, "Enable or disable webhooks been the P2:MM Server and Discord.", true, 0, true, 1, WebhookCheck);
ConVar p2mm_discord_webhook_url("p2mm_discord_webhook_url", "", FCVAR_ARCHIVE | FCVAR_HIDDEN, "Channel webhook URL to send messages to. Should be set in launcher, not here.");
ConVar p2mm_discord_webhook_defaultfooter("p2mm_discord_webhook_defaultfooter", "1", FCVAR_ARCHIVE, "Enable or disable the default embed footer for webhooks.", true, 0, true, 1);
ConVar p2mm_discord_webhook_customfooter("p2mm_discord_webhook_customfooter", "", FCVAR_ARCHIVE, "Set a custom embed footer for webhook messages.");

void RPCState(IConVar* var, const char* pOldValue, float flOldValue)
{
	if (!g_P2MMServerPlugin.m_bPluginLoaded) return;
	ConVar* pRPC = (ConVar*)var;
	if (pRPC->GetBool() && !g_pDiscordIntegration->RPCRunning)
		g_pDiscordIntegration->StartDiscordRPC();
	if (!pRPC->GetBool() && g_pDiscordIntegration->RPCRunning)
		g_pDiscordIntegration->ShutdownDiscordRPC();
}
ConVar p2mm_discord_rpc("p2mm_discord_rpc", "1", FCVAR_ARCHIVE, "Enable or disable Discord RPC with P2:MM.", true, 0, true, 1, RPCState);
ConVar p2mm_discord_rpc_appid("p2mm_discord_rpc_appid", "1201562647880015954", FCVAR_DEVELOPMENTONLY, "Application ID used for Discord RPC with P2:MM.");

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
	if (p2mm_discord_webhook.GetBool())
	{
		P2MMLog(0, true, "Webhook for \"p2mm_discord_webhook_url\" has not been specified.");
		return 1;
	}

	CURL* curl = curl_easy_init();
	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!curl)
	{
		P2MMLog(1, false, "Failed to initalize curl request!");
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, p2mm_discord_webhook_url.GetString());
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
//// OLD API Source: https://github.com/discord/discord-rpc
//// OLD API Documentation: https://github.com/discord/discord-api-docs/tree/legacy-gamesdk/docs/rich_presence
////-----------------------------------------------------------------------------

CDiscordIntegration::CDiscordIntegration()
{
	this->RPCRunning = false; // Flag bool for whether the RPC is running.
	DiscordRichPresence* newRPC = new DiscordRichPresence;
	newRPC->state = "";
	newRPC->details = "Starting up...";
	newRPC->startTimestamp = time(0);
	newRPC->endTimestamp = 0;
	newRPC->largeImageKey = "p2mmlogo";
	newRPC->largeImageText = "Portal 2";
	newRPC->smallImageKey = "";
	newRPC->smallImageText = "";
	newRPC->partyId = "";
	newRPC->partySize = 0;
	newRPC->partyMax = 0;
	newRPC->matchSecret = "";
	newRPC->joinSecret = "";
	newRPC->spectateSecret = "";
	newRPC->instance = 0;
	this->RPC = newRPC;
}

CDiscordIntegration::~CDiscordIntegration()
{
	delete RPC;
	this->RPC = NULL;
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

	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));

	handlers.ready = HandleDiscordReady;
	handlers.disconnected = HandleDiscordDisconnected;
	handlers.errored = HandleDiscordError;
	handlers.joinGame = HandleDiscordJoin;
	handlers.spectateGame = HandleDiscordSpectate;
	handlers.joinRequest = HandleDiscordJoinRequest;

	char appid[255];
	V_snprintf(appid, 255, "%d", engineServer->GetAppID());
	Discord_Initialize(p2mm_discord_rpc_appid.GetString(), &handlers, 1, appid);

	if (g_P2MMServerPlugin.iCurGameIndex == 1)
	{
		this->RPC->largeImageKey = "p2mmmellogo";
		this->RPC->largeImageText = "Portal Stories: Mel";
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
	DiscordLog(0, true, "Discord RPC Debug Spew:");
	DiscordLog(0, true, "state: %s", g_pDiscordIntegration->RPC->state);
	DiscordLog(0, true, "details: %s", g_pDiscordIntegration->RPC->details);
	DiscordLog(0, true, "startTimestamp: %I64d", g_pDiscordIntegration->RPC->startTimestamp);
	DiscordLog(0, true, "endTimestamp: %I64d", g_pDiscordIntegration->RPC->endTimestamp);
	DiscordLog(0, true, "largeImageKey: %s", g_pDiscordIntegration->RPC->largeImageKey);
	DiscordLog(0, true, "largeImageText: %s", g_pDiscordIntegration->RPC->largeImageText);
	DiscordLog(0, true, "smallImageKey: %s", g_pDiscordIntegration->RPC->smallImageKey);
	DiscordLog(0, true, "smallImageText: %s", g_pDiscordIntegration->RPC->smallImageText);
	DiscordLog(0, true, "partyId: %s", g_pDiscordIntegration->RPC->partyId);
	DiscordLog(0, true, "partySize: %i", g_pDiscordIntegration->RPC->partySize);
	DiscordLog(0, true, "partyMax: %i", g_pDiscordIntegration->RPC->partyMax);
	DiscordLog(0, true, "matchSecret: %s", g_pDiscordIntegration->RPC->matchSecret);
	DiscordLog(0, true, "joinSecret: %s", g_pDiscordIntegration->RPC->joinSecret);
	DiscordLog(0, true, "spectateSecret: %s", g_pDiscordIntegration->RPC->spectateSecret);
	DiscordLog(0, true, "instance: %i", g_pDiscordIntegration->RPC->spectateSecret);
	Discord_UpdatePresence(this->RPC);
}

//---------------------------------------------------------------------------------
// Purpose: Discord RPC loop for updating presence and recieving and requests from the Discord Client.
//---------------------------------------------------------------------------------
// This is currently unused for the moment.
void DiscordRPCLoop()
{
	if (!g_pGlobals)
		return;

#ifdef DISCORD_DISABLE_IO_THREAD
	Discord_UpdateConnection();
#endif
	Discord_RunCallbacks();
}