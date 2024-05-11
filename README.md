# Portal 2: Multiplayer Mod Plugin

## **Created by Nanoman2525 & NULLderef. Maintained by Orsell.**

The Portal 2 Source Engine server plugin used to run the 2.2.x+ versions of the Portal 2: Multiplayer Mod.

**This plugin currently only works for Windows. Only designed to work with Portal 2. Portal 2 based mods may work but haven't been tested.**

This plugin has been put into a separate repository due to the nature of the development and compiling environment of Source Engine plugins. The plugin alone can not make the Portal 2: Multiplayer Mod work. If you are looking to play the Portal 2: Multiplayer Mod itself, please look at this repository instead: <https://github.com/Portal-2-Multiplayer-Mod/Portal-2-Multiplayer-Mod>

The purpose of this plugin is to patch Portal 2 to make the mod work as well as fix some bugs that impact multiplayer sessions. The plugin also provides access to features of the Source Engine directly that can be interfaced with by VScript.

## VScript Functions Added To Interface With The Engine:

```c++
const char* GetPlayerName(int index)                 | "Gets player username by index."
int         GetSteamID(int index)                    | "Gets the account ID component of player SteamID by index."
bool        IsMapValid(const char* map)              | "Returns true is the supplied string is a valid map name."
int         GetDeveloperLevelP2MM()                  | "Returns the value of ConVar p2mm_developer."
void        SetPhysTypeConvar(int newval)            | "Sets 'player_held_object_use_view_model' to the supplied integer value."
void        SetMaxPortalSeparationConvar(int newval) | "Sets 'portal_max_separation_force' to the supplied integer value."
bool        IsDedicatedServer()                      | "Returns true if this is a dedicated server."
void        InitializeEntity(HSCRIPT ent)            | "Initializes an entity."
void        SendToChat(const char* msg, int index)   | "Sends a raw message to the chat HUD."
const char* GetGameDirectory()                       | "Returns the game directory."
const char* GetLastMap()                             | "Returns the last map recorded by the launcher's Last Map system."
bool        IsFirstRun()                             | "Returns true if this is the first map ever run during the game session."
```
