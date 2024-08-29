// discord.hpp
#ifndef DISCORD_HPP
#define DISCORD_HPP
#include <string>
const std::string webhookUrl = "https://discord.com/api/webhooks/1277701763461414944/AxsqHZRWee71-lO0OPV8ruRLt9rjf1iyOX4AgTae8xp3SeTSo4LL6gPW5bJJzX5dMgYu"; // Replace with your actual webhook URL
void sendMessageToDiscord(const char* author, const char* message, int color);

#endif // DISCORD_HPP
