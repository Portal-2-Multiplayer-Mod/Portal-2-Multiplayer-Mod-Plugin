//===========================================================================//
//
// Author: \n & Orsell
// Purpose: Discord RPC and Webhook Integration
// 
//===========================================================================//

#include "globals.hpp"
#include "discordrpc.hpp"

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <discord/discord.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Generates a footer with the player count with max allowed client count and also the current map name
std::string DefaultFooter()
{	
	// gpGlobals doesn't exist yet at certain situations
	if (!gpGlobals) { return ""; }

	std::string curplayercount = std::to_string(CURPLAYERCOUNT());
	std::string maxplayercount = std::to_string(gpGlobals->maxClients);

	std::string footer = std::string("Players: ") + curplayercount + "/" + maxplayercount + std::string(" || Current Map: ") + CURMAPNAME;

	return footer;
}

// Discord integration ConVars
ConVar p2mm_discord_webhook("p2mm_discord_webhook", "https://discord.com/api/webhooks/1283885221367779350/c64Md9duQCN6kwodqrsGnBCceDNOyAHvfyKY4s9RK94a7w1Sg1Pavna1FDSdjvgQ5y7c", FCVAR_HIDDEN, "Channel webhook URL to send messages to. Should be set in launcher, not here.");
ConVar p2mm_discord_webhook_defaultfooter("p2mm_discord_webhook_defaultfooter", "1", FCVAR_NONE, "Enable or disable the default embed footer for webhooks.", true, 0, true, 1);
ConVar p2mm_discord_webhook_customfooter("p2mm_discord_webhook_customfooter", "", FCVAR_NONE, "Set a custom embed footer for webhook messages.");
ConVar p2mm_discord_rpc("p2mm_discord_rpc", "1", FCVAR_NONE, "Enable or disable Discord RPC with P2:MM.", true, 0, true, 1);
ConVar p2mm_discord_rpc_appid("p2mm_discord_rpc_appid", "1201562647880015954", FCVAR_HIDDEN, "Application ID used for Discord RPC with P2:MM.");

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
	if (sizeof(p2mm_discord_webhook.GetString()) <= 0)
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
		P2MMLog(1, false, std::to_string(curlCode).c_str());
		P2MMLog(1, false, "Failed to send curl request!");
	}

	P2MMLog(0, true, "Send webhook curl request!");

	// Cleanup curl request
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	curl_global_cleanup();

	// Free the messages parameters from memory
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