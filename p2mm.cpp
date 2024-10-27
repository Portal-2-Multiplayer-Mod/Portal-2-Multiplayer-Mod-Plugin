//===========================================================================//
//
// Author: Nanoman2525 & NULLderef
// Maintainer: Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
// Note: This plugin was made in like 20 mins, but everything works.
// 
//===========================================================================//

#include "p2mm.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
IVEngineServer* engineServer = NULL; // Access engine server functions (messaging clients, loading content, making entities, running commands, etc)
IVEngineClient* engineClient = NULL; // Access engine client functions
CGlobalVars* g_pGlobals = NULL; // Access global variables shared between the engine and games dlls
IPlayerInfoManager* playerinfomanager = NULL; // Access interface functions for players
IScriptVM* g_pScriptVM = NULL; // Access VScript interface
IServerTools* g_pServerTools = NULL; // Access to interface from engine to tools for manipulating entities
IGameEventManager2* gameeventmanager_ = NULL; // Access game events interface
IServerPluginHelpers* pluginHelpers = NULL; // Access interface for plugin helper functions
IFileSystem* g_pFileSystem = NULL; // Access interface for Valve's file system interface
#ifndef GAME_DLL
#define gameeventmanager gameeventmanager_
#endif

//---------------------------------------------------------------------------------
// The plugin is a static singleton that is exported as an interface
//---------------------------------------------------------------------------------
CP2MMServerPlugin g_P2MMServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CP2MMServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_P2MMServerPlugin);

// List of game events the plugin interfaces used to load each one
static const char* gameevents[] =
{
	"portal_player_ping",
	"portal_player_portaled",
	"turret_hit_turret",
	"security_camera_detached",
	"player_landed",
	"player_death",
	"player_connect",
	"player_say",
	"player_activate",
};

// List of console commands that clients can't execute but the host can
static const char* forbiddenconcommands[] =
{
	"mp_earn_taunt",
	"mp_mark_all_maps_complete",
	"mp_mark_all_maps_incomplete",
	"mp_mark_course_complete",
	"report_entities",
	"script", // Valve patched this, here just in case
	"script_debug", // Valve patched this, here just in case
	"script_dump_all", // Valve patched this, here just in case
	"script_execute", // Valve patched this, here just in case
	"script_help", // Valve patched this, here just in case
	"script_reload_code", // Valve patched this, here just in case
	"script_reload_entity_code", // Valve patched this, here just in case
	"script_reload_think", // Valve patched this, here just in case
	"ent_fire",
	"fire_rocket_projectile",
	"fire_energy_ball",
	"ent_remove",
	"ent_remove_all"
};

// List of client commands that need to be blocked from client execution, but can be executed by the host
static const char* forbiddenclientcommands[] =
{
	"taunt_auto", // Apparently mp_earn_taunt calls this also to ClientCommand
	"restart_level",
	"pre_go_to_hub",
	"pre_go_to_calibration",
	"go_to_calibration",
	"go_to_hub",
	"restart_level",
	"mp_restart_level",
	"transition_map",
	"select_map",
	"mp_select_level",
	"erase_mp_progress",
	"bugpause",
	"bugunpause"
};

// Core P2:MM ConVars | These shouldn't be modfied manually. Hidden to prevent accidentally breaking something
ConVar p2mm_loop("p2mm_loop", "0", FCVAR_HIDDEN, "Flag if P2MMLoop should be looping.");
ConVar p2mm_lastmap("p2mm_lastmap", "", FCVAR_HIDDEN, "Last map recorded for the Last Map system.");
ConVar p2mm_splitscreen("p2mm_splitscreen", "0", FCVAR_HIDDEN, "Flag for the main menu buttons and launcher to start in splitscreen or not.");

// UTIL ConVars | ConVars the host can change.
ConVar p2mm_forbidclientcommands("p2mm_forbidclientcommands", "1", FCVAR_NONE, "Stop client commands clients shouldn't be executing.");
ConVar p2mm_deathicons("p2mm_deathicons", "1", FCVAR_NONE, "Whether or not when players die the death icon should appear.");
ConVar p2mm_ds_enable_paint("p2mm_ds_enable_paint", "0", FCVAR_NONE, "Re-enables gel functionality in dedicated servers on the next map load.");

// Debug ConVars
ConVar p2mm_developer("p2mm_developer", "0", FCVAR_NONE, "Enable for P2:MM developer messages.");
ConVar p2mm_spewgameeventinfo("p2mm_spewgameevents", "0", FCVAR_NONE, "Log information from called game events in the console, p2mm_developer must also be on. Can cause lots of console spam.");

// ConCommands

std::vector<std::string> mapList; // List of maps for the p2mm_command auto complete.
std::vector<std::string> workshopMapList; // List of all workshop map for the p2mm_command auto complete.

void updateMapsList() {
	mapList.clear();
	CUtlVector<CUtlString> outList;
	AddFilesToList(outList, "maps", "GAME", "bsp");

	FOR_EACH_VEC(outList, i)
	{
		// Get each map and get their relative path to each SearchPath and make slashes forward slashes.
		// Then turn relativePath into a std::string to easily manipulate.
		const char* curmap = outList[i];
		char relativePath[MAX_PATH];
		g_pFileSystem->FullPathToRelativePathEx(curmap, "GAME", relativePath, sizeof(relativePath));
		V_FixSlashes(relativePath, '/');
		V_StripExtension(relativePath, relativePath, sizeof(relativePath));
		std::string fixedRelativePath(relativePath);

		// Remove "maps/" out of the string.
		fixedRelativePath.erase(0, strlen("maps/"));

		// Remove the whole "workshop/(workshop id)" part if there isn't multiple workshop maps of the same file name.
		size_t lastSlashPos = fixedRelativePath.find_last_of("/");
		if (lastSlashPos != std::string::npos && fixedRelativePath.rfind("workshop/") != std::string::npos)
		{
			fixedRelativePath.erase(0, strlen("workshop/"));
			workshopMapList.push_back(fixedRelativePath);
		}

		// Push the map string on to the list to display avaliable options for the command.
		mapList.push_back(std::move(fixedRelativePath));
		P2MMLog(0, true, mapList.back().c_str());
	}
}

static int p2mm_startsession_CompletionFunc(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	// If the map list is empty, generate it.
	if (mapList.empty()) {
		updateMapsList();
	}

	// Assemble together the current state of the inputted command.
	const char* concommand = "p2mm_startsession ";
	const char* match = (V_strstr(partial, concommand) == partial) ? partial + V_strlen(concommand) : partial;

	// Go through the map list searching for matches with the assembled inputted command.
	int numMatchedMaps = 0;
	for (const std::string map : mapList)
	{
		if (numMatchedMaps >= COMMAND_COMPLETION_MAXITEMS) break;

		if (V_strstr(map.c_str(), match))
		{
			V_snprintf(commands[numMatchedMaps++], COMMAND_COMPLETION_ITEM_LENGTH, "%s%s", concommand, map.c_str());
			P2MMLog(0, true, map.c_str());
		}
	}

	return numMatchedMaps;
}

CON_COMMAND(p2mm_updatemaplist, "Manually updates the list of avaliable maps that can be loaded with \"p2mm_startsession\"")
{
	updateMapsList();
}

CON_COMMAND(p2mm_maplist, "Lists avaliable maps that can be loaded with \"p2mm_startsession\"")
{
	P2MMLog(0, false, "AVALIABLE MAPS:");
	P2MMLog(0, false, "----------------------------------------");
	for (const std::string map : mapList)
	{
		P2MMLog(0, false, map.c_str());
	}
	P2MMLog(0, false, "----------------------------------------");
}

CON_COMMAND_F_COMPLETION(p2mm_startsession, "Starts up a P2:MM session with a requested map.", 0, p2mm_startsession_CompletionFunc)
{
	// Make sure the CON_COMMAND was executed correctly.
	if (args.ArgC() < 2 || FStrEq(args.Arg(1), ""))
	{
		P2MMLog(1, false, "p2mm_startsession called incorrectly! Usage: 'p2mm_startsession (map to start) '");
		updateMapsList();
		return;
	}

	// A check done by the menu to request to use the last recorded map in the p2mm_lastmap ConVar.
	const char* requestedMap = args.Arg(1);
	P2MMLog(0, true, "Requested Map: %s", requestedMap);
	P2MMLog(0, true, "p2mm_lastmap: %s", p2mm_lastmap.GetString());
	if (FSubStr(requestedMap, "P2MM_LASTMAP"))
	{
		if (!engineServer->IsMapValid(p2mm_lastmap.GetString()))
		{
			// Running disconnect to make the error screen appear causes the music to stop, don't let that to happen, so here it is start it again.

			// Get the current act so we can start the right main menu music
			int iAct = ConVarRef("ui_lastact_played").GetInt();
			if (iAct > 5) iAct = 5;
			else if (iAct < 1) iAct = 1;

			// Put the command to start the music and the act number together.
			char completePVCmd[sizeof("playvol \"#music/mainmenu/portal2_background0%d\" 0.35") + sizeof(iAct)];
			V_snprintf(completePVCmd, sizeof(completePVCmd), "playvol \"#music/mainmenu/portal2_background0%i\" 0.35", iAct);

			P2MMLog(1, false, "p2mm_session was called with P2MM_LASTMAP, but p2mm_lastmap is empty or invalid!");
			engineClient->ExecuteClientCmd("disconnect \"There is no last map recorded or the map doesn't exist! Please start a play session with the other options first.\"");
			engineClient->ExecuteClientCmd(completePVCmd);
			updateMapsList();
			return;
		}
		requestedMap = p2mm_lastmap.GetString();
		P2MMLog(0, true, "P2MM_LASTMAP called! Running Last Map: \"%s\"", requestedMap);
	}
	p2mm_lastmap.SetValue(""); // Set last map ConVar to blank so it doesn't trigger level changes where we don't want it to trigger.

	// Check if the requested map is a workshop map.
	std::string tempMapStr = requestedMap;
	for (const std::string map : workshopMapList)
	{
		if (tempMapStr == map)
		{
			tempMapStr = std::string("workshop/" + tempMapStr);
			requestedMap = tempMapStr.c_str();
		}
	}

	// Check if the supplied map is a valid map.
	if (!engineServer->IsMapValid(requestedMap))
	{
		P2MMLog(1, false, "p2mm_startsession was given a non-valid map or one that doesn't exist! \"%s\"", requestedMap);
		updateMapsList();
		return;
	}

	// Check if the user requested it to start in splitscreen or not.
	std::string mapString = p2mm_splitscreen.GetBool() ? "ss_map " : "map ";
	P2MMLog(0, true, "Map String: %s", mapString.c_str());

	// Set first run flag on and set the last map ConVar value so the system.
	// can change from mp_coop_community_hub to the requested map.
	// Also set m_bSeenFirstRunPrompt back to false so the prompt can be triggered again.
	g_P2MMServerPlugin.m_bFirstMapRan = true;
	g_P2MMServerPlugin.m_bSeenFirstRunPrompt = false;
	if (!FSubStr(requestedMap, "mp_coop"))
	{
		P2MMLog(0, true, "'mp_coop' not found, singleplayer map being run. Full ExecuteClientCmd: \"%s\"", std::string(mapString + "mp_coop_community_hub").c_str());
		P2MMLog(0, true, "requestedMap: \"%s\"", requestedMap);
		p2mm_lastmap.SetValue(requestedMap);
		engineClient->ExecuteClientCmd(std::string(mapString + "mp_coop_community_hub").c_str());
	}
	else
	{
		P2MMLog(0, true, "'mp_coop' found, multiplayer map being run. Full ExecuteClientCmd: \"%s\"", std::string(mapString + requestedMap).c_str());
		P2MMLog(0, true, "requestedMap: \"%s\"", requestedMap);
		engineClient->ExecuteClientCmd(std::string(mapString + requestedMap).c_str());
	}
}

CON_COMMAND(p2mm_respawnall, "Respawns all players.")
{
	for (int i = 1; i < g_pGlobals->maxClients; i++)
	{
		CPortal_Player__RespawnPlayer(i);
	}
}

//---------------------------------------------------------------------------------
// Purpose: constructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::CP2MMServerPlugin()
{
	this->m_iClientCommandIndex = 0;

	// Store plugin Status
	this->m_bPluginLoaded = false;
	this->m_bNoUnload = false;				// If we fail to load, we don't want to run anything on Unload()

	// Store game vars
	this->m_bSeenFirstRunPrompt = false;	// Flag is set true after CallFirstRunPrompt() is called in VScript
	this->m_bFirstMapRan = true;			// Checks if the game ran for the first time

	m_nDebugID = EVENT_DEBUG_ID_INIT;
}

//---------------------------------------------------------------------------------
// Purpose: destructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::~CP2MMServerPlugin()
{
	m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
}

//---------------------------------------------------------------------------------
// Purpose: Description of plugin outputted when the "plugin_print" console command is executed.
//---------------------------------------------------------------------------------
const char* CP2MMServerPlugin::GetPluginDescription(void)
{
	return "Portal 2: Multiplayer Mod Server Plugin | Plugin Version: " P2MM_PLUGIN_VERSION " | For P2:MM Version: " P2MM_VERSION;
}

// NoSteamLogon stop hook. Suposively Valve fixed this, again, but this will be here just in case.
//void(__fastcall* disconnect_orig)(void *thisptr, void *edx, void *cl, void *eDenyReason, const char *pchOptionalText);
//void __fastcall disconnect_hook(void *thisptr, void *edx, void *cl, void *eDenyReason, const char *pchOptionalText)
//{	
//	if ((int)eDenyReason == 0xC)
//		return;
//
//	disconnect_orig(thisptr, edx, cl, eDenyReason, pchOptionalText);
//}


// Bottom three hooks are for being able to change the starting models to something different.
// First two are for changing what model is returned when precaching however...
// Last one is for actually specifying the right model as the MSVC compiler inlined the returns
// of the original functions of these first two hooks. Thanks MSVC for making this hard. :D
const char* (__cdecl* GetBallBotModel_orig)(bool bLowRes);
const char* __cdecl GetBallBotModel_hook(bool bLowRes)
{
	if (FStrEq(GFunc::GetGameMainDir(), "portal_stories"))
	{
		return "models/portal_stories/player/mel.mdl";
	}

	return GetBallBotModel_orig(bLowRes);
}

const char* (__cdecl* GetEggBotModel_orig)(bool bLowRes);
const char* __cdecl GetEggBotModel_hook(bool bLowRes)
{
	if (FStrEq(GFunc::GetGameMainDir(), "portal_stories"))
	{
		return "models/player/chell/player.mdl";
	}

	return GetEggBotModel_orig(bLowRes);
}

const char* (__fastcall* CPortal_Player__GetPlayerModelName_orig)(CPortal_Player* thisptr);
const char* __fastcall CPortal_Player__GetPlayerModelName_hook(CPortal_Player* thisptr) {
	if (FStrEq(GFunc::GetGameMainDir(), "portal_stories"))
	{
		if (CBaseEntity__GetTeamNumber((CBasePlayer*)thisptr) == TEAM_BLUE)
			return "models/portal_stories/player/mel.mdl";
		else
			return "models/player/chell/player.mdl";		
	}
	return CPortal_Player__GetPlayerModelName_orig(thisptr);
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is loaded, initialization process.
//			Loads the interfaces we need from the engine and applies our patches.
//---------------------------------------------------------------------------------
bool CP2MMServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	if (m_bPluginLoaded)
	{
		P2MMLog(1, false, "Already loaded!");
		m_bNoUnload = true;
		return false;
	}

	P2MMLog(0, false, "Loading plugin...");

	P2MMLog(0, true, "Connecting tier libraries...");
	ConnectTier1Libraries(&interfaceFactory, 1);
	ConnectTier2Libraries(&interfaceFactory, 1);

	// Make sure that all the interfaces needed are loaded and useable
	P2MMLog(0, true, "Loading interfaces...");
	engineServer = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, 0);
	if (!engineServer)
	{
		P2MMLog(1, false, "Unable to load engineServer!");
		this->m_bNoUnload = true;
		return false;
	}

	engineClient = (IVEngineClient*)interfaceFactory(VENGINE_CLIENT_INTERFACE_VERSION, 0);
	if (!engineClient)
	{
		P2MMLog(1, false, "Unable to load engineClient!");
		this->m_bNoUnload = true;
		return false;
	}

	playerinfomanager = (IPlayerInfoManager*)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, 0);
	if (!playerinfomanager)
	{
		P2MMLog(1, false, "Unable to load playerinfomanager!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pScriptVM = (IScriptVM*)interfaceFactory(VSCRIPT_INTERFACE_VERSION, 0);
	if (!g_pScriptVM)
	{
		P2MMLog(1, false, "Unable to load g_pScriptVM!");
		this->m_bNoUnload = true;
		return false;
	}
	
	g_pServerTools = (IServerTools*)gameServerFactory(VSERVERTOOLS_INTERFACE_VERSION, 0);
	if (!g_pServerTools)
	{
		P2MMLog(1, false, "Unable to load g_pServerTools!");
		this->m_bNoUnload = true;
		return false;
	}

	gameeventmanager = (IGameEventManager2*)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2, 0);
	if (!gameeventmanager)
	{
		P2MMLog(1, false, "Unable to load gameeventmanager!");
		this->m_bNoUnload = true;
		return false;
	}

	pluginHelpers = (IServerPluginHelpers*)interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, 0);
	if (!pluginHelpers)
	{
		P2MMLog(1, false, "Unable to load pluginHelpers!");
		this->m_bNoUnload = true;
		return false;
	}
	 
	g_pFileSystem = (IFileSystem*)interfaceFactory(FILESYSTEM_INTERFACE_VERSION, 0);
	if (!g_pFileSystem)
	{
		P2MMLog(1, false, "Unable to load g_pFileSystem!");
		this->m_bNoUnload = true;
		return false;
	}

	g_pGlobals = playerinfomanager->GetGlobalVars();
	MathLib_Init(2.2f, 2.2f, 0.0f, 2.0f);
	ConVar_Register(0);

	// Add listener for all used game events
	P2MMLog(0, true, "Adding listeners for game events...");
	for (const char* gameevent : gameevents)
	{
		gameeventmanager->AddListener(this, gameevent, true);
		P2MMLog(0, true, "Listener for gamevent '%s' has been added!", gameevent);
	}

	P2MMLog(0, true, "Blocking console commands...");
	for (const char* concommand : forbiddenconcommands)
	{
		ConCommandBase* commandbase = g_pCVar->FindCommandBase(concommand);
		if (commandbase)
		{
			commandbase->RemoveFlags(FCVAR_GAMEDLL);
		}
	}
	
	// big ol' try catch because game has a TerminateProcess handler for exceptions...
	// why this wasn't here is mystyfying, - 10/2024 NULLderef
	try {
		// Byte patches
		P2MMLog(0, true, "Patching Portal 2...");

		// Linked portal doors event crash patch
		Memory::ReplacePattern("server", "0F B6 87 04 05 00 00 8B 16", "EB 14 87 04 05 00 00 8B 16"); // crashes infra, bytes incorrect

		// Partner disconnects
		Memory::ReplacePattern("server", "51 50 FF D2 83 C4 10 E8", "51 50 90 90 83 C4 10 E8"); // crashes infra, bytes incorrect
		Memory::ReplacePattern("server", "74 28 3B 75 FC", "EB 28 3B 75 FC"); // crashes infra, bytes incorrect

		// Max players -> 33
		Memory::ReplacePattern("server", "83 C0 02 89 01", "83 C0 20 89 01");
		Memory::ReplacePattern("engine", "85 C0 78 13 8B 17", "31 C0 04 21 8B 17");
		static uintptr_t sv = *reinterpret_cast<uintptr_t*>(Memory::Scanner::Scan<void*>(ENGINEDLL, "74 0A B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B E5", 3));
		*reinterpret_cast<int*>(sv + 0x228) = 33;

		// Prevent disconnect by "STEAM validation rejected"
		Memory::ReplacePattern("engine", "01 74 7D 8B", "01 EB 7D 8B"); // doesn't immedatley crash infra but does later, bytes incorrect

		// Fix sv_password
		Memory::ReplacePattern("engine", "0F 95 C1 51 8D 4D E8", "03 C9 90 51 8D 4D E8"); // doesn't immedatley crash infra but does later, bytes incorrect

		// runtime max 0.03 -> 0.05
		Memory::ReplacePattern("vscript", "00 00 00 E0 51 B8 9E 3F", "9a 99 99 99 99 99 a9 3f"); // doesn't immedatley crash infra but does later, bytes incorrect

		// Make sure -allowspectators is there so we get our 33 max players
		if (!CommandLine()->FindParm("-allowspectators"))
		{
			CommandLine()->AppendParm("-allowspectators", "");
		}

		// MinHook initallization and hooking
		P2MMLog(0, true, "Initalizing MinHook and hooking functions...");
		MH_Initialize();
		// NoSteamLogon disconnect hook patch.
		//MH_CreateHook((LPVOID)Memory::Scanner::Scan<void*>(ENGINEDLL, "55 8B EC 83 EC 08 53 56 57 8B F1 E8 ?? ?? ?? ?? 8B"), &disconnect_hook, (LPVOID*)&disconnect_orig);
	
		// Hook onto the function which defines what Atlas's and PBody's models are.
		MH_CreateHook(
			Memory::Rel32(Memory::Scanner::Scan(SERVERDLL, "E8 ?? ?? ?? ?? 83 C4 40 50", 1)),
			&GetBallBotModel_hook, (void**)&GetBallBotModel_orig);
		MH_CreateHook(
			Memory::Rel32(Memory::Scanner::Scan(SERVERDLL, "E8 ?? ?? ?? ?? 83 C4 04 50 8B 45 10 8B 10", 1)),
			&GetEggBotModel_hook, (void**)&GetEggBotModel_orig);
		MH_CreateHook(
			Memory::Scanner::Scan(SERVERDLL, "55 8B EC 81 EC 10 01 00 00 53 8B 1D"),
			&CPortal_Player__GetPlayerModelName_hook,
			(void**)&CPortal_Player__GetPlayerModelName_orig
		);
	
		MH_EnableHook(MH_ALL_HOOKS);

		P2MMLog(0, false, "Loaded plugin! Horray!");
		m_bPluginLoaded = true;
	} catch(std::exception& ex) {
		P2MMLog(0, false, "Failed to load plugin! :( Exception: (%s)", ex.what());
		this->m_bNoUnload = true;
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is turning off/unloading.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::Unload(void)
{
	// If the plugin errors for some reason, prevent it from unloading.
	if (m_bNoUnload)
	{
		m_bNoUnload = false;
		return;
	}

	P2MMLog(0, false, "Unloading Plugin...");

	P2MMLog(0, true, "Removing listeners for game events...");
	gameeventmanager->RemoveListener(this);

	ConVar_Unregister();
	P2MMLog(0, true, "Disconnecting tier libraries...");
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();

	// Undo byte patches
	P2MMLog(0, true, "Unpatching Portal 2...");

	// Linked portal doors event crash patch
	Memory::ReplacePattern("server", "EB 14 87 04 05 00 00 8B 16", "0F B6 87 04 05 00 00 8B 16");

	// Partner disconnects
	Memory::ReplacePattern("server", "51 50 90 90 83 C4 10 E8", "51 50 FF D2 83 C4 10 E8");
	Memory::ReplacePattern("server", "EB 28 3B 75 FC", "74 28 3B 75 FC");

	// Max players -> 3
	Memory::ReplacePattern("server", "83 C0 20 89 01", "83 C0 02 89 01");
	Memory::ReplacePattern("engine", "31 C0 04 21 8B 17", "85 C0 78 13 8B 17");
	static uintptr_t sv = *reinterpret_cast<uintptr_t*>(Memory::Scanner::Scan<void*>(ENGINEDLL, "74 0A B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B E5", 3));
	*reinterpret_cast<int*>(sv + 0x228) = 2;

	// Disconnect by "STEAM validation rejected"
	Memory::ReplacePattern("engine", "01 EB 7D 8B", "01 74 7D 8B");

	// sv_password
	Memory::ReplacePattern("engine", "03 C9 90 51 8D 4D E8", "0F 95 C1 51 8D 4D E8");

	// runtime max 0.05 -> 0.03
	Memory::ReplacePattern("vscript", "00 00 00 00 00 00 E0 3F", "00 00 00 E0 51 B8 9E 3F");

	P2MMLog(0, true, "Disconnecting hooked functions and uinitalizing MinHook...");
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();

	m_bPluginLoaded = false;
	P2MMLog(0, false, "Plugin unloaded! Goodbye!");
}

void CP2MMServerPlugin::SetCommandClient(int index)
{
	m_iClientCommandIndex = index;
}

void RegisterFuncsAndRun();
void CP2MMServerPlugin::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	RegisterFuncsAndRun();
}

//---------------------------------------------------------------------------------
// Purpose: Called when a map has started loading. Does gel patching and Last Map System stuff.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::LevelInit(char const* pMapName)
{
	// Dedicated server paintmap patch
	// Paint usage doesn't function naturally on dedicated servers, so this will help enable it again.
	if (p2mm_ds_enable_paint.GetBool() && engineServer->IsDedicatedServer())
	{
		if (!p2mm_ds_enable_paint.GetBool())
		{
			return;
		}

		// Hook R_LoadWorldGeometry (gl_rmisc.cpp)
		static auto R_LoadWorldGeometry =
#ifdef _WIN32
			reinterpret_cast<void(__cdecl*)(bool bDXChange)>(Memory::Scanner::Scan<void*>(ENGINEDLL, "55 8B EC 83 EC 14 53 33 DB 89"));
#else
			NULL; // TODO: Linux & MacOS
#endif //  _WIN32
		if (R_LoadWorldGeometry)
		{
			CUtlVector< uint32 > paintData;
			engineServer->GetPaintmapDataRLE(paintData);
			R_LoadWorldGeometry(false);
			CUtlVector< uint32 > paintData2;
			engineServer->GetPaintmapDataRLE(paintData2);
		}
		else
		{
			P2MMLog(1, true, "Couldn't find R_LoadWorldGeometry! Paint will not work on this map load.");
		}
	}
}

//---------------------------------------------------------------------------------
// Purpose: Called when a client inputs a console command.
//---------------------------------------------------------------------------------
PLUGIN_RESULT CP2MMServerPlugin::ClientCommand(edict_t* pEntity, const CCommand& args)
{
	if (!pEntity || pEntity->IsFree())
	{
		return PLUGIN_CONTINUE;
	}

	bool spewinfo = p2mm_spewgameeventinfo.GetBool();
	const char* pcmd = args[0];
	const char* fargs = args.ArgS();
	
	short userid = engineServer->GetPlayerUserId(pEntity);
	int entindex = GFunc::UserIDToPlayerIndex(userid);
	const char* playername = GFunc::GetPlayerName(entindex);

	if (spewinfo)
	{
		P2MMLog(0, true, "ClientCommand called: %s", pcmd);
		P2MMLog(0, true, "ClientCommand args: %s", fargs);
		P2MMLog(0, true, "userid: %i", userid);
		P2MMLog(0, true, "entindex: %i", entindex);
		P2MMLog(0, true, "playername: %s", playername);
		P2MMLog(0, true, "VScript VM Working?: %s", (g_pScriptVM != NULL) ? "Working" : "Not Working!");
	}

	// Call the "GEClientCommand" VScript function
	if (g_pScriptVM)
	{
		HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEClientCommand");
		if (ge_func)
		{
			g_pScriptVM->Call<const char*, const char*, short, int, const char*>(ge_func, NULL, true, NULL, pcmd, fargs, userid, entindex, playername);
		}
	}

	// signify is the client command used to make on screen icons appear
	if (FStrEq(pcmd, "signify"))
	{
		// Check if its the death icons and if the death icons disable ConVar is on
		if ((FStrEq(args[1], "death_blue") || FStrEq(args[1], "death_orange")) && !p2mm_deathicons.GetBool())
		{
			return PLUGIN_STOP;
		}
	}

	// Stop certain client commands from being excecated by clients and not the host
	for (const char* badcc : forbiddenclientcommands)
	{
		if (FSubStr(pcmd, "taunt_auto") || FSubStr(pcmd, "mp_earn_taunt"))
		{
			return PLUGIN_STOP;
		}
		if (entindex != 1 && (FSubStr(pcmd, badcc)) && p2mm_forbidclientcommands.GetBool())
		{
			engineServer->ClientPrintf(INDEXENT(entindex), "This command is blocked from execution!\n");
			return PLUGIN_STOP;
		}
	}

	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: Capture and work with game events. Interfaces game events to VScript functions.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::FireGameEvent(IGameEvent* event)
{
	bool spewinfo = p2mm_spewgameeventinfo.GetBool();
	if (spewinfo)
	{
		P2MMLog(0, true, "Game Event Fired: %s", event->GetName());
		P2MMLog(0, true, "VScript VM Working?: %s", (g_pScriptVM != NULL) ? "Working" : "Not Working!");
	}

	// Event called when a player pings, "portal_player_ping" returns:
	/*
		"userid"	"short"		// user ID on server
		"ping_x"	"float"		// ping's x-coordinate in map
		"ping_y"	"float"		// ping's y-coordinate in map
		"ping_z"	"float"		// ping's z-coordinate in map
	*/
	if (FStrEq(event->GetName(), "portal_player_ping"))
	{
		short userid = event->GetInt("userid");
		float ping_x = event->GetFloat("ping_x");
		float ping_y = event->GetFloat("ping_y");
		float ping_z = event->GetFloat("ping_z");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerPing");
			if (ge_func)
			{
				g_pScriptVM->Call<short, float, float, float, int>(ge_func, NULL, true, NULL, userid, ping_x, ping_y, ping_z, entindex);
			}
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "ping_x: %f", ping_x);
			P2MMLog(0, true, "ping_y: %f", ping_y);
			P2MMLog(0, true, "ping_z: %f", ping_z);
			P2MMLog(0, true, "entindex: %i", entindex);
		}
		
		return;
	}
	// Event called when a player goes through a portal, "portal_player_portaled" returns:
	/*
		"userid"	"short"		// user ID on server
		"portal2"	"bool"		// false for portal1 (blue)
	*/
	else if (FStrEq(event->GetName(), "portal_player_portaled"))
	{
		short userid = event->GetInt("userid");
		bool portal2 = event->GetString("text");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerPortaled");
			if (ge_func)
			{
				g_pScriptVM->Call<short, bool, int>(ge_func, NULL, true, NULL, userid, portal2, entindex);
			}
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "portal2: %s", portal2 ? "true" : "false");
			P2MMLog(0, true, "entindex: %i", entindex);
		}
		
		return;
	}
	// Event called when a player goes through a portal, "turret_hit_turret" returns nothing.
	else if (FStrEq(event->GetName(), "turret_hit_turret"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GETurretHitTurret");
			if (ge_func)
			{
				g_pScriptVM->Call(ge_func, NULL, true, NULL);
			}
		}

		return;
	}
	// Event called when a player goes through a portal, "security_camera_detached" returns nothing.
	else if (FStrEq(event->GetName(), "security_camera_detached"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GECamDetach");
			if (ge_func)
			{
				g_pScriptVM->Call(ge_func, NULL, true, NULL);
			}
		}
		return;
	}
	// Event called when a player touches the ground, "player_landed" returns:	
	/*
		"userid"	"short"		// user ID on server
	*/
	else if (FStrEq(event->GetName(), "player_landed"))
	{
		short userid = event->GetInt("userid");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerLanded");
			if (ge_func)
			{
				g_pScriptVM->Call<short, int>(ge_func, NULL, true, NULL, userid, entindex);
			}
		}

		return;
	}
	// Event called when a player dies, "player_death" returns:	
	/*
		"userid"	"short"   	// user ID who died
		"attacker"	"short"	 	// user ID who killed
	*/
	else if (FStrEq(event->GetName(), "player_death"))
	{
		short userid = event->GetInt("userid");
		short attacker = event -> GetInt("attacker");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handling OnDeath VScript event
			HSCRIPT od_func = g_pScriptVM->LookupFunction("OnDeath");
			if (od_func)
			{
				HSCRIPT playerHandle = INDEXHANDLE(entindex);
				if (playerHandle)
				{
					// player does not have a script scope yet, fire OnPlayerJoin
					g_pScriptVM->Call<HSCRIPT>(od_func, NULL, true, NULL, playerHandle);
				}
			}

			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerDeath");
			if (ge_func)
			{
				g_pScriptVM->Call<short, short, int>(ge_func, NULL, true, NULL, userid, attacker, entindex);
			}
		}
		
		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "attacker: %i", attacker);
			P2MMLog(0, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player connects to the server, "player_connect" returns:
	/*
		"name"		"string"	// player name
		"index"		"byte"		// player slot (entity index-1)
		"userid"	"short"		// user ID on server (unique on server) "STEAM_1:...", will be "BOT" if player is bot
		"xuid"		"uint64"	// XUID/Steam ID (converted to const char*)
		"networkid" "string" 	// player network (i.e steam) id
		"address"	"string"	// ip:port
		"bot"		"bool"		// player is a bot
	}
	*/
	else if (FStrEq(event->GetName(), "player_connect"))
	{
		const char* name = event->GetString("name");
		int index = event->GetInt("index");
		short userid = event->GetInt("userid");
		const char* xuid = std::to_string(event->GetUint64("xuid")).c_str();
		const char* networkid = event->GetString("networkid");
		const char* address = event->GetString("address");
		bool bot = event->GetBool("bot");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerConnect");
			if (ge_func)
			{
				g_pScriptVM->Call<const char*, int, short, const char*, const char*, const char*, bool, int>(ge_func, NULL, true, NULL, name, index, userid, xuid, networkid, address, bot, entindex);
			}
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "name: %s", name);
			P2MMLog(0, true, "index: %i", index);
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "xuid: %d", xuid);
			P2MMLog(0, true, "networkid: %s", networkid);
			P2MMLog(0, true, "address: %s", address);
			P2MMLog(0, true, "bot: %i", bot);
			P2MMLog(0, true, "entindex: %i", entindex);
		}
		
		return;
	}
	// Event called when a player changes their name, "player_info" returns:
	/*
		"name"		"string"	// player name
		"index"		"byte"		// player slot (entity index-1)
		"userid"	"short"		// user ID on server (unique on server) "STEAM_1:...", will be "BOT" if player is bot
		"friendsid" "short"		// friends identification number
		"networkid"	"string"	// player network (i.e steam) id
		"bot"		"bool"		// true if player is a AI bot
	*/
	else if (FStrEq(event->GetName(), "player_info"))
	{
		const char* name = event->GetString("name");
		int index = event->GetInt("index");
		short userid = event->GetInt("userid");
		const char* networkid = event->GetString("networkid");
		const char* address = event->GetString("address");
		bool bot = event->GetBool("bot");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerInfo");
			if (ge_func)
			{
				g_pScriptVM->Call<const char*, int, short, const char*, const char*, bool, int>(ge_func, NULL, true, NULL, name, index, userid, networkid, address, bot, entindex);
			}
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "name: %s", name);
			P2MMLog(0, true, "index: %i", index);
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "networkid: %s", networkid);
			P2MMLog(0, true, "address: %s", address);
			P2MMLog(0, true, "bot: %i", bot);
			P2MMLog(0, true, "entindex: %i", entindex);
		}
		
		return;
	}
	// Event called when a player inputs a message into the chat, "player_say" returns:
	/*
		"userid"	"short"		// user ID on server
		"text"		"string"	// the say text
	*/
	else if (FStrEq(event->GetName(), "player_say"))
	{
		short userid = event->GetInt("userid");
		const char* text = event->GetString("text");
		int entindex = GFunc::UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			if (entindex != NULL)
			{
				// Handling chat commands
				HSCRIPT cc_func = g_pScriptVM->LookupFunction("ChatCommands");
				if (cc_func)
				{
					g_pScriptVM->Call<const char*, int>(cc_func, NULL, true, NULL, text, entindex);
				}
			}
			
			// Handle VScript game event function
			HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEPlayerSay");
			if (ge_func)
			{
				g_pScriptVM->Call<short, const char*, int>(ge_func, NULL, true, NULL, userid, text, entindex);
			}
			
		}

		if (spewinfo)
		{
			P2MMLog(0, true, "userid: %i", userid);
			P2MMLog(0, true, "text: %s", text);
			P2MMLog(0, true, "entindex: %i", entindex);
		}
		
		return;
	}

	return;
}

//---------------------------------------------------------------------------------
// Purpose: Called when a player is "activated" in the server, meaning fully loaded, not fully connect which happens before that.
// Called when game event "player_activate" is also called so this is used to call "GEClientActive".
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::ClientActive(edict_t* pEntity)
{
	short userid = engineServer->GetPlayerUserId(pEntity);
	int entindex = GFunc::UserIDToPlayerIndex(userid);

	P2MMLog(0, true, "ClientActive Called!");
	P2MMLog(0, true, "userid: %i", userid);
	P2MMLog(0, true, "entindex: %i", entindex);

	if (g_pScriptVM)
	{
		// Handling OnPlayerJoin VScript event
		HSCRIPT opj_func = g_pScriptVM->LookupFunction("OnPlayerJoin");
		if (opj_func)
		{
			HSCRIPT playerHandle = INDEXHANDLE(entindex);
			if (playerHandle)
			{
				// player does not have a script scope yet, fire OnPlayerJoin
				g_pScriptVM->Call<HSCRIPT>(opj_func, NULL, true, NULL, playerHandle);
			}
		}

		// Handle VScript game event function
		HSCRIPT ge_func = g_pScriptVM->LookupFunction("GEClientActive");
		if (ge_func)
		{
			g_pScriptVM->Call<short, int>(ge_func, NULL, true, NULL, userid, entindex);
		}
	}

	return;
}

//---------------------------------------------------------------------------------
// Purpose: Called every server frame, used for the VScript loop. Warning: Don't do too intensive tasks with this!
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::GameFrame(bool simulating)
{
	HSCRIPT loop_func = g_pScriptVM->LookupFunction("P2MMLoop");
	if (loop_func && p2mm_loop.GetBool())
	{
		g_pScriptVM->Call(loop_func, NULL, true, NULL);
	}

	// Handle VScript game event function
	HSCRIPT gf_func = g_pScriptVM->LookupFunction("GEGameFrame");
	if (gf_func)
	{
		g_pScriptVM->Call<bool>(gf_func, NULL, true, NULL, simulating);
	}
}

//---------------------------------------------------------------------------------
// Purpose: Called when a the map is changing to another map, or the server is shutting down.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::LevelShutdown(void)
{
	p2mm_loop.SetValue("0"); // REMOVE THIS
}

//---------------------------------------------------------------------------------
// Purpose: Unused callbacks
//---------------------------------------------------------------------------------
#pragma region UNUSED_CALLBACKS
void CP2MMServerPlugin::Pause(void) {}
void CP2MMServerPlugin::UnPause(void) {}
void CP2MMServerPlugin::ClientDisconnect(edict_t* pEntity) {}
void CP2MMServerPlugin::ClientFullyConnect(edict_t* pEntity) {} // Purpose: Called when a player is fully connected to the server. Player entity still has not spawned in so manipulation is not possible.
void CP2MMServerPlugin::ClientPutInServer(edict_t* pEntity, char const* playername) {}
void CP2MMServerPlugin::ClientSettingsChanged(edict_t* pEdict) {}
PLUGIN_RESULT CP2MMServerPlugin::ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) { return PLUGIN_CONTINUE; }
PLUGIN_RESULT CP2MMServerPlugin::NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) {}
void CP2MMServerPlugin::OnEdictAllocated(edict_t* edict) {}
void CP2MMServerPlugin::OnEdictFreed(const edict_t* edict) {}
bool CP2MMServerPlugin::BNetworkCryptKeyCheckRequired(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey) { return false; }
bool CP2MMServerPlugin::BNetworkCryptKeyValidate(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte* pbEncryptedBufferFromClient, byte* pbPlainTextKeyForNetchan) { return true; }
#pragma endregion