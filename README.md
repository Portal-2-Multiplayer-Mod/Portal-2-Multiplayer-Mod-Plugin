# Portal 2: Multiplayer Mod Plugin

## **Created by Nanoman2525 & NULLderef. Maintained by Orsell.**

The Portal 2 Source Engine server plugin used to run the 2.2+ versions of the Portal 2: Multiplayer Mod.

**This plugin currently only works for Windows. Only designed to work with Portal 2. Portal 2 based mods may work but haven't been tested.**

This plugin has been put into a separate repository due to the nature of the development and compiling environment of Source Engine plugins. The plugin alone can not make the Portal 2: Multiplayer Mod work. If you are looking to play the Portal 2: Multiplayer Mod itself, please look at this repository instead: <https://github.com/Portal-2-Multiplayer-Mod/Portal-2-Multiplayer-Mod>

The purpose of this plugin is to patch Portal 2 to make the mod work as well as fix some bugs that impact multiplayer sessions. The plugin also provides access to features of the Source Engine directly that can be interfaced with by VScript. The added VScript functions are used to access Source Engine interfaces and ConVars. The interfaced game event VScript functions are called by plugin and can be used to do certain things based on those game events. `portal2allgameevents.res` in the main repository (<https://github.com/Portal-2-Multiplayer-Mod/Portal-2-Multiplayer-Mod/blob/dev/mapmaking/portal2allgameevents.res>) also lists what game events Portal 2 has and which ones the plugin interfaces. When game events are called, the plugins operations take priority over the interfaced VScript calls.

## VScript Functions Added By C++ Plugin To Interface With The Engine:

Note: While represented in C++, below functions are for Squirrel. `const char*` type is translated to `string` type. `byte`, `short`, and `long` types are translated to `integer` type. `void` types are simply the `function` type in Squirrel. `HSCRIPT` is a VScript script handle which is an entity instance.

```c++
void        printlP2MM(int level, bool dev, const char* pMsgFormat);   | "Logging for the P2MM VScript. The log message must be passed as a string or it will error."
const char* GetPlayerName(int playerIndex);                            | "Gets player username by their entity index."
int         GetSteamID(int playerIndex);                               | "Gets the account ID component of player SteamID by the player's entity index."
int         UserIDToPlayerIndex(int userid);                           | "Get the player's entity index by their userid."
bool        IsMapValid(const char* map);                               | "Returns true is the supplied string is a available map to load and run."
int         GetDeveloperLevelP2MM();                                   | "Returns the value of ConVar p2mm_developer."
void        SetPhysTypeConVar(int newval);                             | "Sets 'player_held_object_use_view_model' to the supplied integer value."
void        SetMaxPortalSeparationConvar(int newval);                  | "Sets 'portal_max_separation_force' to the supplied integer value."
bool        IsDedicatedServer();                                       | "Returns true if this is a dedicated server."
void        InitializeEntity(HSCRIPT ent);                             | "Initializes an entity. Note: Not all entities will work even after being initialized with this function."
void        SendToChat(const char* msg, int playerIndex);              | "Sends a raw message to the chat HUD. Specifying no playerIndex or 0 sends to all players. Supports printing localization strings but those that require formatting can't be formatted."
const char* GetGameMainDir();                                          | "Returns the game directory. Ex. portal2"
const char* GetGameBaseDir();                                          | "Get the main game directory being used. Ex. Portal 2"
const char* GetLastMap();                                              | "Returns the last map recorded by the launcher's Last Map system."
bool        FirstRunState();                                           | "Get or set the state of whether the first map was run or not. Set false/true = 0/1 | -1 to get state."
void        CallFirstRunPrompt();                                      | "Shows the first run prompt if enabled in config.nut."
int         GetConVarInt(const char* cvName);                          | "Get the integer value of a ConVar."
const char* GetConVarString(const char* cvName);                       | "Get the string value of a ConVar."
void        SetConVarInt(const char* cvName, int newValue);            | "Set the integer value of a ConVar."
void        SetConVarString(const char* cvName, const char* newValue); | "Set the string value of a ConVar."
HSCRIPT     UTIL_PlayerByIndex(int playerIndex);                       | "Takes the player's entity index and returns the player's script handle."
void        RespawnPlayer(int playerIndex);                            | "Respawn the a player by their entity index."
void        SetFlashlightState(int playerIndex, bool enable);          | "Set the flashlight for a player on or off."
void        ConsolePrint(int playerIndex, const char* msg);            | "Print a message to a player's console, unlike printl() which is just the host. Specifying no playerIndex or 0 sends to all players. Supports printing localization strings but those that require formatting can't be formatted."
void        ClientPrint(int playerIndex, const char* msg);             | "Print a message to the top center position of a player's screen. Specifying no playerIndex or 0 sends to all players. Supports printing localization strings but those that require formatting can't be formatted."
void        HudPrint(int playerIndex, const char* msg, Vector(float x, float y, int channel), int effect, float fxTime, Vector RGB1, float alpha1, Vector RGB2, float alpha2, Vector(float fadeinTime, float fadeoutTime, float holdTime));
                                                                       | "Print a message to the screen based on what the game_text entity does, with many values to set. See Valve Developer Commentary for the game_text entity to see what each field does. Vectors are in place for sets of RGB values. Specifying no playerIndex or 0 sends to all players. Supports printing localization strings but those that require formatting can't be formatted."
int         GetMaxPlayers();                                           | "Self-explanatory."
void        ShowScoreboard(int playerIndex, bool bEnable);             | "Enable or disable displaying the score board a player."
```

## Game Events Interfaced To Squirrel VScript Functions:

The "game events" listed at the top of `portal2allgameevents.res` are not actually game events but are some plugin callbacks or other events interfaced to VScript so they can be utilized.

`GEClientCommand` is the plugin's ClientCommand callback function.

`GEClientActive` is the plugin's ClientActive callback function.

`GEGameFrame` is the plugin's GameFrame callback function.

`GEPlayerRespawn` is called by the hooked respawn function the game uses to respawn players.

```c++
void GEClientCommand(short userid, int entindex, const char* pcmd, const char* fargs);      | "Called when a client inputs a console command."
void GEClientActive(short userid, int entindex);                                            | "Called when a player is 'activated' in the server, meaning fully loaded. This is not the same as fully connect which happens before ClientActive."
void GEGameFrame(bool simulating);                                                          | "Called every server frame, used for the VScript loop. Warning: Don't do too intensive tasks with this!"
void GEPlayerRespawn(HSCRIPT playerHandle);                                                 | "Called when a player respawns."
void GEPlayerPing(short userid, float ping_x, float ping_y, float ping_z, int entindex);    | "Called whenever a player pings. Game event: 'portal_player_ping'"
void GEPlayerPortaled(short userid, bool portal2, int entindex);                            | "Called whenever a player goes through a portal. `portal2` is false when portal1/blue portal is entered. Game event: 'portal_player_portaled'"
void GETurretHitTurret();                                                                   | "Called whenever a turret hits another turret. Game event: 'turret_hit_turret'"
void GECamDetach();                                                                         | "Called whenever a camera is detached from a surface. Game event: 'security_camera_detached'"
void GEPlayerLanded(short userid, int entindex);                                            | "Called whenever a player lands on the ground. Game event: 'player_landed'"
void GEPlayerSpawnBlue();                                                                   | "Called whenever a Blue/Atlas player spawns. Game event: 'player_spawn_blue'"
void GEPlayerSpawnOrange();                                                                 | "Called whenever a Red/Orange/PBody player spawns. Game event: 'player_spawn_orange'"
void GEPlayerDeath(short userid, short attacker, int entindex);                             | "Called whenever a player dies. Game event: 'player_death'"
void GEPlayerSpawn(short userid, int entindex);                                             | "Called whenever a player spawns. Game event: 'player_spawn'"
void GEPlayerConnect(const char* name, int index, short userid, const char* xuid, 
                const char* networkid, const char* address, bool bot, int entindex);        | "Called where a player connects to the server. 'index' is the entity index minus 1. Game event: 'player_connect'"
void GEPlayerInfo(const char* name, int index, short userid, const char* networkid,
                 const char* address, bool bot, int entindex);                              | "Called when a player changes their name."
void GEPlayerSay(short userid, const char* text, int entindex);                             | "Called whenever a player inputs a chat message. Game event: 'player_say'"
```
