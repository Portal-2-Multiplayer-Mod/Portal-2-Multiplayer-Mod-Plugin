#include "discord.hpp"
#include "p2mm.hpp"
#include <iostream>
#include <string>
#include <curl/curl.h>
extern ConVar p2mm_bridge_webhook;
void sendMessageToDiscord(const char* author, const char* message, int color) {
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, p2mm_bridge_webhook.GetString());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // im fucking losing it
        // im fucking losing it
        // im fucking losing it
        // Create the JSON payload
        char jsonPayload[284]; 
        V_snprintf(jsonPayload, sizeof(jsonPayload), "{ \"content\": null, \"embeds\" : [ {\"title\": \"%s\", \"description\" : \"%s\", \"color\" : %i }] , \"attachments\": [] }", author, message, color);
        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload);

        // Set the Content-Type header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            P2MMLog(1, false, "curl_easy_perform() failed");
        }
        else {
            P2MMLog(1, false, "message sent!");
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}
