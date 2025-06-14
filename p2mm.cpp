//===========================================================================//
//
// Author: Nanoman2525 & NULLderef
// Maintainer: Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//
#include "p2mm.hpp"

#include "scanner.hpp"
#include "sdk.hpp"
#include "commands.hpp"
#include "discordrpc.hpp"

#include "minhook/include/MinHook.h"

#include <Windows.h>

extern ConVar p2mm_discord_rpc;
extern ConVar p2mm_discord_webhooks;

//---------------------------------------------------------------------------------
// Interfaces from the engine
//---------------------------------------------------------------------------------
IVEngineServer* engineServer = nullptr; // Access engine server functions (messaging clients, loading content, making entities, running commands, etc).
IVEngineClient* engineClient = nullptr; // Access engine client functions.
CGlobalVars* g_pGlobals = nullptr; // Access global variables shared between the engine and games dlls.
IPlayerInfoManager* g_pPlayerInfoManager = nullptr; // Access interface functions for players.
IScriptVM* g_pScriptVM = nullptr; // Access VScript interface.
IServerTools* g_pServerTools = nullptr; // Access to interface from engine to tools for manipulating entities.
IGameEventManager2* g_pGameEventManager_ = nullptr; // Access game events interface.
IServerPluginHelpers* g_pPluginHelpers = nullptr; // Access interface for plugin helper functions.
IFileSystem* g_pFileSystem = nullptr; // Access interface for Valve's file system interface.
#ifndef GAME_DLL
#define g_pGameEventManager g_pGameEventManager_
#endif

//---------------------------------------------------------------------------------
// Class declarations/creations
//---------------------------------------------------------------------------------

//---------------------------------------------------------------------------------
// The plugin is a static singleton that is exported as an interface
//---------------------------------------------------------------------------------
CP2MMServerPlugin g_P2MMServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CP2MMServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_P2MMServerPlugin);

// List of game events the plugin interfaces used to load each one.
static const char* gameEventList[] =
{
	"portal_player_ping",
	"portal_player_portaled",
	"turret_hit_turret",
	"security_camera_detached",
	"player_landed",
	"player_spawn_blue",
	"player_spawn_orange",
	"player_death",
	"player_spawn",
	"player_connect",
	"player_say",
	"player_activate",
};

// List of console commands that clients can't execute but the host can.
static const char* forbiddenConCommands[] =
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

// List of client commands that need to be blocked from client execution, but can be executed by the host.
static const char* forbiddenClientCommands[] =
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

//---------------------------------------------------------------------------------
// Purpose: constructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::CP2MMServerPlugin()
{
	this->m_hWnd = nullptr; // Game window handle
	
	// Store game vars
	this->m_bSeenFirstRunPrompt = false;	// Flag is set true after CallFirstRunPrompt() is called in VScript.
	this->m_bFirstMapRan = true;			// Checks if the game ran for the first time.
	this->sv = nullptr;						// Pointer to the server.

	// Store plugin status
	this->m_bPluginLoaded = false;
	this->m_bPluginUnloading = false;		// For Discord RPC.
	this->m_bNoUnload = false;				// If we fail to load, we don't want to run anything on Unload().
	this->m_bP2SMPluginLoaded = false;		// Some functions that the Portal 2 SourceMod++ plugin does match some fixes 

	// Current Portal 2 branch based game being run.
	// Helps when checking for specific game related things instead of getting the game directory everytime.
	this->m_iCurGameIndex = -1;

	m_nDebugID = EVENT_DEBUG_ID_INIT;
	this->m_iClientCommandIndex = 0;
}

//---------------------------------------------------------------------------------
// Purpose: destructor
//---------------------------------------------------------------------------------
CP2MMServerPlugin::~CP2MMServerPlugin()
{
	this->m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
}

//---------------------------------------------------------------------------------
// Purpose: Description of plugin outputted when the "plugin_print" console command is executed.
//---------------------------------------------------------------------------------
const char* CP2MMServerPlugin::GetPluginDescription(void)
{
	return "Portal 2: Multiplayer Mod Server Plugin | Plugin Version: " P2MM_PLUGIN_VERSION " | For P2:MM Version: " P2MM_VERSION;
}

//---------------------------------------------------------------------------------
// Purpose: Called when the plugin is loaded, initialization process.
//			Loads the interfaces we need from the engine and applies our patches.
//---------------------------------------------------------------------------------
bool CP2MMServerPlugin::Load(CreateInterfaceFn interfaceFactory, const CreateInterfaceFn gameServerFactory)
{
	if (m_bPluginLoaded)
	{
		Log(WARNING, false, "Plugin already loaded!");
		return false;
	}

	Log(INFO, false, "Loading plugin...");

	Log(INFO, true, "Grabbing game window handle...");
	this->m_hWnd = FindWindow("Valve001", nullptr);
	if (!this->m_hWnd)
		Log(WARNING, false, "Failed to find game window!");

// MARK: Determine Current Game
#pragma region Determine Current Game
	// Determine which Portal 2 branch game we are running and if its supported.
	// TODO: Rework this to be a switch case then if and else if statements.
	bool unsupportedGame = false;
	const char* gameMainDir = GetGameMainDir();
	Log(INFO, true, "Determining which Portal 2 branch game is being run...");
	if ((FStrEq(gameMainDir, "portal2")))
	{
		this->m_iCurGameIndex = PORTAL_2;
		Log(INFO, false, "Currently running Portal 2!");
	}
	else if ((FStrEq(gameMainDir, "portal_stories")))
	{
		this->m_iCurGameIndex = PORTAL_STORIES_MEL;
		Log(INFO, false, "Currently running Portal Stories: Mel!");
	}
	else if ((FStrEq(gameMainDir, "aperturetag")))
	{
		this->m_iCurGameIndex = APERTURE_TAG;
		Log(INFO, false, "Currently running Aperture Tag!");
	}
	else if ((FStrEq(gameMainDir, "portalreloaded")))
	{
		this->m_iCurGameIndex = PORTAL_RELOADED;
		Log(INFO, false, "Currently running Portal Reloaded!");
		// Unsupported...
		unsupportedGame = true;
	}
	// else if ((FStrEq(gameMainDir, "infra")))
	// {
	// 	this->m_iCurGameIndex = INFRA;
	// 	Log(INFO, false, "Currently running Infra.");
	// 	// Unsupported...
	// 	unsupportedGame = true;
	// }
	// else if ((FStrEq(gameMainDir, "thestanleyparable")))
	// {
	// 	this->m_iCurGameIndex = STANLEY_PARABLE;
	// 	Log(INFO, false, "Currently running The Stanley Parable.");
	// 	// Unsupported...
	// 	unsupportedGame = true;
	// }
	// Have to find "Divinity" in the gameMainDir string as it's a big folder path due to being a SourceMod.
	else if ((std::strstr(gameMainDir, "Divinity")))
	{
		this->m_iCurGameIndex = DIVINITY;
		Log(INFO, false, "Currently running Portal: Divinity!");
	}
	else if (!CommandLine()->FindParm("-forcep2mmload"))
	{
		Log(ERRORR, false, "\nAn unsupported Source Engine/Portal 2 branch game has been started with P2:MM! Please check the FAQ to see which Portal 2 engine based games are supported!");
		return false;
	}
	else
		unsupportedGame = true;

	if (unsupportedGame && !CommandLine()->FindParm("-forcep2mmload"))
	{
		Log(ERRORR, false, "\nThe current Source Engine/Portal 2 branch game is not **yet** supported by P2:MM! Please check the FAQ to see which games are supported!");
		return false;
	}
	if (unsupportedGame && CommandLine()->FindParm("-forcep2mmload"))
	{
		if (this->m_hWnd)
			MessageBox(this->m_hWnd, "P2:MM is being run with a unsupported Source Engine/Portal 2 branch game!\n\"-forcep2mmload\" has been specified to stop the plugin shutting down the game.\nProceed with caution as crashes and bugs could occur!", "Unsupported P2:MM Game", MB_OK | MB_ICONEXCLAMATION);
		Log(WARNING, false, R"(P2:MM is being run with a unsupported Source Engine/Portal 2 branch game! "-forcep2mmload" has been specified to stop the plugin shutting down the game. Proceed with caution as crashes and bugs could occur!)");
	}
#pragma endregion

	Log(INFO, true, "Connecting tier libraries...");
	ConnectTier1Libraries(&interfaceFactory, 1);
	ConnectTier2Libraries(&interfaceFactory, 1);

// MARK: Engine Interfaces
#pragma region Engine Interfaces
	// Make sure that all the interfaces needed are loaded and usable.
	Log(INFO, true, "Loading interfaces...");
	Log(INFO, true, "Loading engineServer...");
	engineServer = static_cast<IVEngineServer*>(interfaceFactory(INTERFACEVERSION_VENGINESERVER, 0));
	if (!engineServer)
	{
		assert(0 && "Unable to load engineServer!");
		Log(WARNING, false, "Unable to load engineServer!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading engineClient...");
	engineClient = static_cast<IVEngineClient*>(interfaceFactory(VENGINE_CLIENT_INTERFACE_VERSION, 0));
	if (!engineClient)
	{
		assert(0 && "Unable to load engineClient!");
		Log(WARNING, false, "Unable to load engineClient!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading g_pPlayerInfoManager...");
	g_pPlayerInfoManager = static_cast<IPlayerInfoManager*>(gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, 0));
	if (!g_pPlayerInfoManager)
	{
		assert(0 && "Unable to load g_pPlayerInfoManager!");
		Log(WARNING, false, "Unable to load g_pPlayerInfoManager!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading g_pServerTools...");
	g_pServerTools = static_cast<IServerTools*>(gameServerFactory(VSERVERTOOLS_INTERFACE_VERSION, 0));
	if (!g_pServerTools)
	{
		assert(0 && "Unable to load g_pServerTools!");
		Log(WARNING, false, "Unable to load g_pServerTools!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading g_pGameEventManager...");
	g_pGameEventManager = static_cast<IGameEventManager2*>(interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2, 0));
	if (!g_pGameEventManager)
	{
		assert(0 && "Unable to load g_pGameEventManager!");
		Log(WARNING, false, "Unable to load g_pGameEventManager!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading g_pPluginHelpers...");
	g_pPluginHelpers = static_cast<IServerPluginHelpers*>(interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, 0));
	if (!g_pPluginHelpers)
	{
		assert(0 && "Unable to load g_pPluginHelpers!");
		Log(WARNING, false, "Unable to load g_pPluginHelpers!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading g_pFileSystem...");
	g_pFileSystem = static_cast<IFileSystem*>(interfaceFactory(FILESYSTEM_INTERFACE_VERSION, 0));
	if (!g_pFileSystem)
	{
		assert(0 && "Unable to load g_pFileSystem!");
		Log(WARNING, false, "Unable to load g_pFileSystem!");
		this->m_bNoUnload = true;
		return false;
	}

	Log(INFO, true, "Loading g_pGlobals...");
	g_pGlobals = g_pPlayerInfoManager->GetGlobalVars();
	if (!g_pGlobals)
	{
		assert(0 && "Unable to load g_pGlobals!");
		Log(WARNING, false, "Unable to load g_pGlobals!");
		this->m_bNoUnload = true;
		return false;
	}
#pragma endregion
	
	MathLib_Init(2.2f, 2.2f, 0.0f, 2.0f);
	ConVar_Register(0);

	// Check if Portal 2 SourceMod++ plugin is loaded. If it is, some functionality will not be enabled in P2:MM as P2SM++ will take priority.
	Log(INFO, true, "Checking if Portal 2 SourceMod++ is loaded...");
	if (g_pCVar->FindVar("p2sm_developer"))
	{
		this->m_bP2SMPluginLoaded = true;
		Log(WARNING, true, "Portal 2 SourceMod++ is loaded! Letting P2SM++ have priority over some patches!");
	}
	
	// Enable Discord RPC if it is not disabled by the host.
	Log(INFO, true, "Checking if Discord RPC should be started...");
	if (p2mm_discord_rpc.GetBool() && !CDiscordIntegration::DiscordRPCRunning() /*&& !this->m_bP2SMPluginLoaded*/)
	{
		Log(INFO, true, "Discord RPC enabled! Starting!");
		CDiscordIntegration::StartDiscordRPC();
	}

	// Add listener for all used game events.
	Log(INFO, true, "Adding listeners for game events...");
	for (const char* gameEvent : gameEventList)
	{
		g_pGameEventManager->AddListener(this, gameEvent, true);
		Log(INFO, true, R"(Listener for game event "%s" has been added!)", gameEvent);
	}

	// Block ConCommands that clients shouldn't execute.
	Log(INFO, true, "Blocking console commands...");
	for (const char* conCommand : forbiddenConCommands)
	{
		if (ConCommandBase* commandBase = g_pCVar->FindCommandBase(conCommand))
			commandBase->RemoveFlags(FCVAR_GAMEDLL);
	}

	// Make sure -allowspectators is there so we get our 33 max players.
	if (!CommandLine()->FindParm("-allowspectators"))
	{
		Log(INFO, true, R"(Missing "-allowspectators" launch parameter, adding!)");
		CommandLine()->AppendParm("-allowspectators", "");
	}

// MARK: Patches and Hooks
#pragma region Patches and Hooks
	// big ol' try catch because game has a TerminateProcess handler for exceptions...
	// why this wasn't here is mystifying, - 10/2024 NULLderef
	try
	{
		// Byte patches
		Log(INFO, true, "Patching Portal 2...");
		
		// Linked portal doors event crash patch.
		if (!this->m_bP2SMPluginLoaded)
		{
			Log(INFO, true, "Fixing linked portal doors...");
			Memory::ReplacePattern("server", "0F B6 87 04 05 00 00 8B 16", "EB 14 87 04 05 00 00 8B 16");

			// Increase runtime max from 0.03 to 0.05.
			// Helps add some more leeway to some things we do in VScript without the engine complaining and shutting down the rest of the script.
			Log(INFO, true, "Patching max runtime for VScript...");
			Memory::ReplacePattern("vscript", "00 00 00 E0 51 B8 9E 3F", "9a 99 99 99 99 99 a9 3f");

			// Skip a line which adds a flag to the player that screws with client side prediction for multiplayer.
			// TODO: Currently can not unpatch 
			Log(INFO, true, "Fixing game_ui...");
			switch (m_iCurGameIndex)
			{
			case (PORTAL_2):
			case (DIVINITY):
				Memory::ReplacePattern("server", "E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? F3 0F 10 40 ?? 6A 00 51 8B CE F3 0F 11 04 24 E8 ?? ?? ?? ?? 8B 87", "E9 00 00 00 00");
				break;
			default:
				Memory::ReplacePattern("server", "E8 ?? ?? ?? ?? 6A 00 51 8B 0D ?? ?? ?? ?? F3 0F 10 41 0C 8B CE F3 0F 11 04 24 E8 ?? ?? ?? ?? 8B 87 2C 0B 00 00", "E9 00 00 00 00");
				break;
			}
		}

		// Partner disconnects.
		Log(INFO, true, "Patching partner disconnect event...");
		Memory::ReplacePattern("server", "51 50 FF D2 83 C4 10 E8", "51 50 90 90 83 C4 10 E8");
		Memory::ReplacePattern("server", "74 28 3B 75 FC", "EB 28 3B 75 FC");

		// Max players -> 33
		Log(INFO, true, "Patching patching max players...");
		Memory::ReplacePattern("server", "83 C0 02 89 01", "83 C0 20 89 01");
		Memory::ReplacePattern("engine", "85 C0 78 13 8B 17", "31 C0 04 21 8B 17");
		uintptr_t svPtr = *static_cast<uintptr_t*>(Memory::Scanner::Scan<void*>(ENGINEDLL, "74 0A B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B E5", 3));
		*reinterpret_cast<int*>(svPtr + 0x228) = 33;

		// Store pointer to the CBaseServer for global access.
		this->sv = reinterpret_cast<CBaseServer*>(svPtr);

		// Prevent disconnect by "STEAM validation rejected".
		Log(INFO, true, R"(Patching "STEAM validation rejected" disconnection...)");
		Memory::ReplacePattern("engine", "01 74 7D 8B", "01 EB 7D 8B");

		// Fix sv_password so passwords can be used on servers.
		Log(INFO, true, "Fixing sv_password...");
		Memory::ReplacePattern("engine", "0F 95 C1 51 8D 4D E8", "03 C9 90 51 8D 4D E8");

		// MinHook initialization and hooking.
		Log(INFO, true, "Initializing MinHook and hooking functions...");
		MH_Initialize();

		// NoSteamLogon disconnect hook patch.
		Log(INFO, true, "Hooking CSteam3Server::OnGSClientDenyHelper...");
		MH_CreateHook(
			Memory::Scanner::Scan<void*>(ENGINEDLL, "55 8B EC 83 EC 08 53 56 57 8B F1 E8 ?? ?? ?? ?? 8B"),
			&CSteam3Server__OnGSClientDenyHelper_hook, reinterpret_cast<void**>(&CSteam3Server__OnGSClientDenyHelper_orig)
		);
		
		if (!this->m_bP2SMPluginLoaded)
		{
			// For p2mm_instantrespawn.
			Log(INFO, true, "Hooking CPortal_Player::PlayerDeathThink...");
			MH_CreateHook(
				Memory::Scanner::Scan(SERVERDLL, "53 8B DC 83 EC 08 83 E4 F0 83 C4 04 55 8B 6B ?? 89 6C 24 ?? 8B EC A1 ?? ?? ?? ?? F3 0F 10 40 ?? F3 0F 58 05 ?? ?? ?? ?? 83 EC 28 56 57 6A 00 51 8B F1 F3 0F 11 04 24 E8 ?? ?? ?? ?? 6A 03"),
				&CPortal_Player__PlayerDeathThink_hook, reinterpret_cast<void**>(&CPortal_Player__PlayerDeathThink_orig)
			);
		}
		
		// UTIL_GetLocalPlayer dedicated server hook crash fix.
		Log(INFO, true, "Hooking UTIL_GetLocalPlayer...");
		MH_CreateHook(
			Memory::Scanner::Scan(SERVERDLL, "8B 15 ?? ?? ?? ?? 8B 4A ?? 33 C0"),
			&UTIL_GetLocalPlayer, reinterpret_cast<void**>(&UTIL_GetLocalPlayer_orig)
		);
		
		// "respawn" function hook for getting a VScript "game event" call out of it.
		Log(INFO, true, "Hooking respawn function call...");
		MH_CreateHook(
			Memory::Scanner::Scan(SERVERDLL, "55 8B EC A1 ?? ?? ?? ?? 80 78 ?? ?? 75 ?? 80 78"),
			&respawn_hook, reinterpret_cast<void**>(&respawn_orig)
		);

		// Game-specific hooks
		switch (g_P2MMServerPlugin.m_iCurGameIndex)
		{
		case PORTAL_STORIES_MEL:
			// Valve's compiler for P2 inlined the GetBallBotModel and GetEggBotModel functions here,
			// so this also needs to be forced to return a different string.
			Log(INFO, true, "Hooking CPortal_Player::GetPlayerModelName...");
			MH_CreateHook(
				Memory::Scanner::Scan(SERVERDLL, "55 8B EC 81 EC 10 01 00 00 53 8B 1D"),
				&CPortal_Player__GetPlayerModelName_hook, reinterpret_cast<void**>(&CPortal_Player__GetPlayerModelName_orig)
			);
			
			// Hook onto the function which defines what Atlas's and PBody's models are.
			Log(INFO, true, "Hooking GetBallBotModel...");
			MH_CreateHook(
				Memory::Rel32(Memory::Scanner::Scan(SERVERDLL, "E8 ?? ?? ?? ?? 83 C4 40 50", 1)),
				&GetBallBotModel_hook, reinterpret_cast<void**>(&GetBallBotModel_orig)
			);
			Log(INFO, true, "Hooking GetEggBotModel...");
			MH_CreateHook(
				Memory::Rel32(Memory::Scanner::Scan(SERVERDLL, "E8 ?? ?? ?? ?? 83 C4 04 50 8B 45 10 8B 10", 1)),
				&GetEggBotModel_hook, reinterpret_cast<void**>(&GetEggBotModel_orig)
			);
			break;
		}
		
		Log(INFO, true, "Enabling hooks...");
		MH_EnableHook(MH_ALL_HOOKS);
	}
	catch (const std::exception& ex)
	{
		assert(0 && "Failed to implement patch or hook!");
		Log(WARNING, false, R"(Failed to load plugin! :( Exception: "%s")", ex.what());
		this->m_bNoUnload = true;
		return false;
	}
#pragma endregion
	
	CDiscordIntegration::UpdateDiscordRPC();
	
	Log(INFO, false, "Loaded plugin! Yay! :D");
	m_bPluginLoaded = true;
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
		MessageBox(this->m_hWnd, "P2:MM ran into a error when starting!\nPlease check the console for more info!", "P2:MM Startup Error", MB_OK | MB_ICONERROR);
		return;
	}

	if (IsGameActive())
	{
		m_bNoUnload = false;
		Log(WARNING, false, "You unloaded the P2:MM plugin mid play session! This will cause issues and crashes!");
		Log(WARNING, false, "You unloaded the P2:MM plugin mid play session! This will cause issues and crashes!");
		Log(WARNING, false, "You unloaded the P2:MM plugin mid play session! This will cause issues and crashes!");
		return;
	}

	Log(INFO, false, "Unloading Plugin...");
	this->m_bPluginUnloading = true;
	CDiscordIntegration::UpdateDiscordRPC();

	Log(INFO, true, "Removing listeners for game events...");
	g_pGameEventManager->RemoveListener(this);

	// Unblock ConCommands that clients shouldn't execute
	Log(INFO, true, "Unblocking console commands...");
	for (const char* conCommand : forbiddenConCommands)
	{
		if (ConCommandBase* commandBase = g_pCVar->FindCommandBase(conCommand))
			commandBase->AddFlags(FCVAR_GAMEDLL);
	}

	// Remove -allowspectators so max player count is indeed back to 2 and not 3.
	if (CommandLine()->FindParm("-allowspectators"))
	{
		Log(INFO, true, R"(Removing "-allowspectators" launch parameter"...)");
		CommandLine()->RemoveParm("-allowspectators");
	}

	Log(INFO, true, "Unregistering ConVars/ConCommands...");
	ConVar_Unregister();
	
	Log(INFO, true, "Disconnecting tier libraries...");
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();

// MARK: Patches and Hooks
#pragma region Patches and Hooks
	try
	{
		// Undo byte patches
		Log(INFO, true, "Un-patching Portal 2...");
		
		// Linked portal doors event crash patch
		if (!this->m_bP2SMPluginLoaded)
		{
			Log(INFO, true, "Un-patching linked portal doors...");
			Memory::ReplacePattern("server", "EB 14 87 04 05 00 00 8B 16", "0F B6 87 04 05 00 00 8B 16");

			// runtime max 0.05 -> 0.03
            Log(INFO, true, "Un-patching max runtime for VScript...");
            Memory::ReplacePattern("vscript", "00 00 00 00 00 00 E0 3F", "00 00 00 E0 51 B8 9E 3F");
		}

		// Partner disconnects
		Log(INFO, true, "Un-patching partner disconnect event...");
		Memory::ReplacePattern("server", "51 50 90 90 83 C4 10 E8", "51 50 FF D2 83 C4 10 E8");
		Memory::ReplacePattern("server", "EB 28 3B 75 FC", "74 28 3B 75 FC");

		// Max players -> 2
		Log(INFO, true, "Un-patching max players...");
		Memory::ReplacePattern("server", "83 C0 20 89 01", "83 C0 02 89 01");
		Memory::ReplacePattern("engine", "31 C0 04 21 8B 17", "85 C0 78 13 8B 17");
		*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this->sv) + 0x228) = 2;

		// Disconnect by "STEAM validation rejected"
		Log(INFO, true, R"(Un-patching "STEAM validation rejected" disconnection...)");
		Memory::ReplacePattern("engine", "01 EB 7D 8B", "01 74 7D 8B");

		// sv_password
		Log(INFO, true, "Unfixing sv_password...");
		Memory::ReplacePattern("engine", "03 C9 90 51 8D 4D E8", "0F 95 C1 51 8D 4D E8");

		Log(INFO, true, "Disconnecting hooked functions and initializing MinHook...");
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}
	catch (const std::exception& ex)
	{
		assert(0 && "Failed to fully unload!");
		Log(WARNING, false, R"(Encountered error when unload plugin! :( Exception: "%s")", ex.what());
		Log(ERRORR, false, "P2:MM failed to unload!\nGame has to be shutdown as possibly some other patches/hooks are still connected which can cause issues!");
	}
#pragma endregion

	if (p2mm_discord_rpc.GetBool() && CDiscordIntegration::DiscordRPCRunning())
		CDiscordIntegration::ShutdownDiscordRPC();

	m_bPluginLoaded = false;
	Log(INFO, false, "Plugin unloaded! Goodbye!");
}

//---------------------------------------------------------------------------------
// Purpose: For ClientCommand.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::SetCommandClient(const int index)
{
	m_iClientCommandIndex = index;
}

//---------------------------------------------------------------------------------
// Purpose: When the server activates, start the P2:MM VScript.
//---------------------------------------------------------------------------------
void RegisterFuncsAndRun();
void CP2MMServerPlugin::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	Log(INFO, true, "edictCount: %i", edictCount);
	Log(INFO, true, "clientMax: %i", clientMax);
	RegisterFuncsAndRun();
}

//---------------------------------------------------------------------------------
// Purpose: Called when a map has started loading.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::LevelInit(char const* pMapName)
{
	Log(INFO, true, "Level Init!");

	// Dedicated server paint map patch
	// Paint usage doesn't function naturally on dedicated servers, so this will help enable it again.
	if (engineServer->IsDedicatedServer())
	{
		// Hook R_LoadWorldGeometry (gl_rmisc.cpp)
		static auto R_LoadWorldGeometry =
#ifdef _WIN32
			reinterpret_cast<void(__cdecl*)(bool bDXChange)>(Memory::Scanner::Scan<void*>(ENGINEDLL, "55 8B EC 83 EC 14 53 33 DB 89"));
#else
			nullptr; // TODO: Linux
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
			Log(WARNING, false, "Couldn't find R_LoadWorldGeometry! Paint will not work on this map load.");
	}

	if (!g_P2MMServerPlugin.m_bSeenFirstRunPrompt) return;

	const auto changeMapStr = std::string("The server has changed the map to: `" + std::string(CUR_MAPFILE_NAME) + "`");
	CDiscordIntegration::SendWebHookEmbed("Server", changeMapStr, EMBED_COLOR_SERVER, false);

	// Update Discord RPC to update current map information.
	CDiscordIntegration::UpdateDiscordRPC();
}

//---------------------------------------------------------------------------------
// Purpose: Called when a client inputs a console command.
//---------------------------------------------------------------------------------
PLUGIN_RESULT CP2MMServerPlugin::ClientCommand(edict_t* pEntity, const CCommand& args)
{
	// Check if it's a valid player edict.
	if (!pEntity || pEntity->IsFree())
		return PLUGIN_CONTINUE;

	const char* pCmd = args[0];
	const char* fArgs = args.ArgS();

	int userid = engineServer->GetPlayerUserId(pEntity);
	int entindex = UserIDToPlayerIndex(userid);
	const char* playername = GetPlayerName(entindex);

	if (p2mm_spew_gameevent_info.GetBool())
	{
		Log(INFO, true, "ClientCommand called: %s", pCmd);
		Log(INFO, true, "ClientCommand args: %s", fArgs);
		Log(INFO, true, "userid: %i", userid);
		Log(INFO, true, "entindex: %i", entindex);
		Log(INFO, true, "playername: %s", playername);
		Log(INFO, true, "VScript VM Working?: %s", (g_pScriptVM) ? "Working" : "Not Working!");
	}

	// Call the "GEClientCommand" VScript function
	if (g_pScriptVM)
	{
		if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEClientCommand"))
			g_pScriptVM->Call<const char*, const char*, int, int, const char*>(geFunc, nullptr, false, nullptr, pCmd, fArgs, userid, entindex, playername);
	}

	// signify is the client command used to make on screen icons appear
	if (FStrEq(pCmd, "signify"))
	{
		// Check if its the death icons and if the death icons disable ConVar is on
		if ((FStrEq(args[1], "death_blue") || FStrEq(args[1], "death_orange")) && !p2mm_deathicons.GetBool())
			return PLUGIN_STOP;
	}

	// Stop certain client commands from being excecated by clients and not the host
	for (const char* badCC : forbiddenClientCommands)
	{
		// These commands can be manually called to make everyone emote,
		// however there are certain other ones we need to let in for players individually to emote.
		if (FSubStr(pCmd, "taunt_auto") || FSubStr(pCmd, "mp_earn_taunt"))
			return PLUGIN_STOP;

		// Whether we want to actually stop client commands or not. Host is always ignored.
		if (entindex != 1 && FSubStr(pCmd, badCC) && p2mm_forbid_clientcommands.GetBool())
		{
			engineServer->ClientPrintf(INDEXENT(entindex), "This command is blocked from execution!\n");
			return PLUGIN_STOP;
		}
	}

	if (FSubStr(pCmd, "removeplayeroperation"))
	{
		RemovePlayerOperation(args[1], V_atoi(args[2]));
		return PLUGIN_STOP;
	}

	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: Capture and work with game events. Interfaces game events to VScript functions.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::FireGameEvent(IGameEvent* event)
{
	bool spewInfo = p2mm_spew_gameevent_info.GetBool();
	if (spewInfo)
	{
		Log(INFO, true, "Game Event Fired: %s", event->GetName());
		Log(INFO, true, "VScript VM Working?: %s", (g_pScriptVM) ? "Working" : "Not Working!");
	}

	// Event called when a player pings, "portal_player_ping" returns:
	/*
		"userid"	"int"		// user ID on server
		"ping_x"	"float"		// ping's x-coordinate in map
		"ping_y"	"float"		// ping's y-coordinate in map
		"ping_z"	"float"		// ping's z-coordinate in map
	*/
	if (FStrEq(event->GetName(), "portal_player_ping"))
	{
		int userid = event->GetInt("userid");
		float ping_x = event->GetFloat("ping_x");
		float ping_y = event->GetFloat("ping_y");
		float ping_z = event->GetFloat("ping_z");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerPing"))
				g_pScriptVM->Call<int, float, float, float, int>(geFunc, nullptr, false, nullptr, userid, ping_x, ping_y, ping_z, entindex);
		}

		if (spewInfo)
		{
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "ping_x: %f", ping_x);
			Log(INFO, true, "ping_y: %f", ping_y);
			Log(INFO, true, "ping_z: %f", ping_z);
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player goes through a portal, "portal_player_portaled" returns:
	/*
		"userid"	"int"		// user ID on server
		"portal2"	"bool"		// false for portal1 (blue)
	*/
	if (FStrEq(event->GetName(), "portal_player_portaled"))
	{
		int userid = event->GetInt("userid");
		bool portal2 = event->GetString("text");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerPortaled"))
				g_pScriptVM->Call<int, bool, int>(geFunc, nullptr, false, nullptr, userid, portal2, entindex);
		}

		if (spewInfo)
		{
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "portal2: %s", portal2 ? "true" : "false");
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a turret hits another turret, "turret_hit_turret" returns nothing.
	if (FStrEq(event->GetName(), "turret_hit_turret"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GETurretHitTurret"))
				g_pScriptVM->Call(geFunc, nullptr, false, nullptr);
		}

		return;
	}
	// Event called when a camera is detached from a wall, "security_camera_detached" returns nothing.
	if (FStrEq(event->GetName(), "security_camera_detached"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GECamDetach"))
				g_pScriptVM->Call(geFunc, nullptr, false, nullptr);
		}

		return;
	}
	// Event called when a player touches the ground, "player_landed" returns:	
	/*
		"userid"	"int"		// user ID on server
	*/
	if (FStrEq(event->GetName(), "player_landed"))
	{
		int userid = event->GetInt("userid");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerLanded"))
				g_pScriptVM->Call<int, int>(geFunc, nullptr, false, nullptr, userid, entindex);
		}

		return;
	}
	// Event called when a Blue/Atlas spawns, "player_spawn_blue" returns nothing.
	if (FStrEq(event->GetName(), "player_spawn_blue"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerSpawnBlue"))
				g_pScriptVM->Call(geFunc, nullptr, false, nullptr);
		}

		return;
	}
	// Event called when a Red/Orange/PBody spawns, "player_spawn_orange" returns nothing.
	if (FStrEq(event->GetName(), "player_spawn_orange"))
	{
		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerSpawnOrange"))
				g_pScriptVM->Call(geFunc, nullptr, false, nullptr);
		}

		return;
	}
	// Event called when a player dies, "player_death" returns:	
	/*
		"userid"	"int"   	// user ID who died
		"attacker"	"int"	 	// user ID who killed
	*/
	if (FStrEq(event->GetName(), "player_death"))
	{
		int userid = event->GetInt("userid");
		int attacker = event->GetInt("attacker");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handling OnDeath VScript event
			if (HSCRIPT od_func = g_pScriptVM->LookupFunction("OnDeath"))
			{
				if (HSCRIPT playerHandle = INDEXHANDLE(entindex))
				{
					g_pScriptVM->Call<HSCRIPT>(od_func, nullptr, false, nullptr, playerHandle);
					std::string playerName = GetPlayerName(entindex);
					CDiscordIntegration::SendWebHookEmbed(playerName + std::string(" Died!"), "", EMBED_COLOR_PLAYERDEATH);
				}
			}

			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerDeath"))
				g_pScriptVM->Call<int, int, int>(geFunc, nullptr, false, nullptr, userid, attacker, entindex);
		}

		if (spewInfo)
		{
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "attacker: %i", attacker);
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player spawns, "player_spawn" returns:	
	/*
		"userid"	"int"		// user ID on server
	*/
	if (FStrEq(event->GetName(), "player_spawn"))
	{
		int userid = event->GetInt("userid");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerSpawn"))
				g_pScriptVM->Call<int, int>(geFunc, nullptr, false, nullptr, userid, entindex);
		}

		if (spewInfo)
		{
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
	// The game event for player's connecting is used instead of the plugin's ClientFullyConnected
	// callback because the game event gives more information than the callback without having to do
	// any extra work to get information.
	// Event called when a player connects to the server, "player_connect" returns:
	/*
		"name"		"string"	// player name
		"index"		"byte"		// player slot (entity index-1)
		"userid"	"int"		// user ID on server (unique on server) "STEAM_1:...", will be "BOT" if player is bot
		"xuid"		"uint64"	// XUID/Steam ID (converted to const char*)
		"networkid" "string" 	// player network (i.e steam) id
		"address"	"string"	// ip:port
		"bot"		"bool"		// player is a bot
	}
	*/
	if (FStrEq(event->GetName(), "player_connect"))
	{
		const char* name = event->GetString("name");
		int index = event->GetInt("index");
		int userid = event->GetInt("userid");
		const char* xuid = std::to_string(event->GetUint64("xuid")).c_str();
		const char* networkid = event->GetString("networkid");
		const char* address = event->GetString("address");
		bool bot = event->GetBool("bot");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerConnect"))
			{
				g_pScriptVM->Call<const char*, int, int, const char*, const char*, const char*, bool, int>(geFunc, nullptr, false, nullptr, name, index, userid, xuid, networkid, address, bot, entindex);
				CDiscordIntegration::SendWebHookEmbed(std::string(name + std::string(" Joined!")), std::string(name + std::string(" joined the server!")));
			}
		}

		if (spewInfo)
		{
			Log(INFO, true, "name: %s", name);
			Log(INFO, true, "index: %i", index);
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "xuid: %d", xuid);
			Log(INFO, true, "networkid: %s", networkid);
			Log(INFO, true, "address: %s", address);
			Log(INFO, true, "bot: %i", bot);
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player changes their name, "player_info" returns:
	/*
		"name"		"string"	// player name
		"index"		"byte"		// player slot (entity index-1)
		"userid"	"int"		// user ID on server (unique on server) "STEAM_1:...", will be "BOT" if player is bot
		"friendsid" "short"		// friends identification number
		"networkid"	"string"	// player network (i.e steam) id
		"bot"		"bool"		// true if player is a AI bot
	*/
	if (FStrEq(event->GetName(), "player_info"))
	{
		const char* name = event->GetString("name");
		int index = event->GetInt("index");
		int userid = event->GetInt("userid");
		const char* networkid = event->GetString("networkid");
		const char* address = event->GetString("address");
		bool bot = event->GetBool("bot");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerInfo"))
				g_pScriptVM->Call<const char*, int, int, const char*, const char*, bool, int>(geFunc, nullptr, false, nullptr, name, index, userid, networkid, address, bot, entindex);
		}

		if (spewInfo)
		{
			Log(INFO, true, "name: %s", name);
			Log(INFO, true, "index: %i", index);
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "networkid: %s", networkid);
			Log(INFO, true, "address: %s", address);
			Log(INFO, true, "bot: %i", bot);
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
	// Event called when a player inputs a message into the chat, "player_say" returns:
	/*
		"userid"	"int"		// user ID on server
		"text"		"string"	// the say text
	*/
	if (FStrEq(event->GetName(), "player_say"))
	{
		int userid = event->GetInt("userid");
		const char* text = event->GetString("text");
		int entindex = UserIDToPlayerIndex(userid);

		if (g_pScriptVM)
		{
			if (entindex)
			{
				// Handling chat commands
				if (HSCRIPT cc_func = g_pScriptVM->LookupFunction("ChatCommands"))
				{
					g_pScriptVM->Call<const char*, int>(cc_func, nullptr, false, nullptr, text, entindex);

					std::string playerName = GetPlayerName(entindex);
					std::string chatMsg = text;

					Log(INFO, true, playerName.c_str());
					Log(INFO, true, chatMsg.c_str());

					// Replace any "\\" characters with "\\\\" so backslashes can exist but not break anything
					size_t pos = 0;
					while ((pos = playerName.find("\\", pos)) != std::string::npos) {
						playerName.replace(pos, 1, std::string("\\\\"));
						pos += std::string("\\\\").length();
					}
					pos = 0;
					while ((pos = chatMsg.find("\\", pos)) != std::string::npos) {
						chatMsg.replace(pos, 1, std::string("\\\\"));
						pos += std::string("\\\\").length();
					}
					if (!chatMsg.starts_with("!"))
						CDiscordIntegration::SendWebHookEmbed(playerName, chatMsg);
				}
			}

			// Handle VScript game event function
			if (HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEPlayerSay"))
				g_pScriptVM->Call<int, const char*, int>(geFunc, nullptr, false, nullptr, userid, text, entindex);
		}

		if (spewInfo)
		{
			Log(INFO, true, "userid: %i", userid);
			Log(INFO, true, "text: %s", text);
			Log(INFO, true, "entindex: %i", entindex);
		}

		return;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Called when a player is "activated" in the server, meaning fully loaded, not fully connect which happens before that.
// Called when game event "player_activate" is also called so this is used to call "GEClientActive".
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::ClientActive(edict_t* pEntity)
{
	const int userID = engineServer->GetPlayerUserId(pEntity);
	const int entIndex = UserIDToPlayerIndex(userID);

	if (p2mm_spew_gameevent_info.GetBool())
	{
		Log(INFO, true, "ClientActive Called!");
		Log(INFO, true, "userid: %i", userID);
		Log(INFO, true, "entindex: %i", entIndex);
	}

	// Make sure people know that the chat is being recorded if webhook is set
	if (p2mm_discord_webhooks.GetBool())
	{
		if (CBasePlayer* pPlayer = UTIL_PlayerByIndex(entIndex))
		{
			Log(INFO, true, "Warning for enabled webhooks sent to player index %i.", entIndex);
			UTIL_ClientPrint(pPlayer, HUD_PRINTTALK, "This lobby has Discord Webhook Integration enabled. All of your in-game messages may be sent to a Discord channel.");
		}
	}

	if (g_pScriptVM)
	{
		// Handling OnPlayerJoin VScript event
		if (const HSCRIPT opjFunc = g_pScriptVM->LookupFunction("OnPlayerJoin"))
		{
			if (const HSCRIPT playerHandle = INDEXHANDLE(entIndex))
				g_pScriptVM->Call<HSCRIPT>(opjFunc, nullptr, false, nullptr, playerHandle);
		}

		// Handle VScript game event function
		if (const HSCRIPT geFunc = g_pScriptVM->LookupFunction("GEClientActive"))
			g_pScriptVM->Call<int, int>(geFunc, nullptr, false, nullptr, userID, entIndex);
	}

	// Update Discord RPC to update player count.
	CDiscordIntegration::UpdateDiscordRPC();
}

//---------------------------------------------------------------------------------
// Purpose: Called every server frame, used for the VScript loop. Warning: Don't do too intensive tasks with this!
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::GameFrame(const bool simulating)
{
	if (const HSCRIPT loopFunc = g_pScriptVM->LookupFunction("P2MMLoop"); p2mm_loop.GetBool())
		g_pScriptVM->Call(loopFunc, nullptr, false, nullptr);
	
	// Handle VScript game event function
	if (const HSCRIPT gfFunc = g_pScriptVM->LookupFunction("GEGameFrame"))
		g_pScriptVM->Call<bool>(gfFunc, nullptr, false, nullptr, simulating);
}

extern void UpdateMapList();
//---------------------------------------------------------------------------------
// Purpose: Called when the map is changing to another map, or the server is shutting down.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::LevelShutdown(void)
{
	Log(INFO, true, "Level Shutdown! Map: %s", CUR_MAPFILE_NAME);
	p2mm_loop.SetValue("0"); //! REMOVE THIS at some point...
	UpdateMapList(); // Update the maps list for p2mm_map.
	// Update Discord RPC to update the level information or to say the host is on the main menu.
	CDiscordIntegration::UpdateDiscordRPC();
}

//---------------------------------------------------------------------------------
// Purpose: Called when a client is in the process of joining the server.
//			Here we check if they are on the ban list or not, and stop their connection if they are.
//			!WARNING!: The player entity has yet to be fully spawned in so manipulation to the entity is not possible.
//---------------------------------------------------------------------------------
PLUGIN_RESULT CP2MMServerPlugin::ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, const int maxrejectlen)
{
	Log(INFO, true, "Player Joined (ClientConnect)! playerInfo:");
	player_info_t playerInfo;
	engineServer->GetPlayerInfo(1,				&playerInfo);
	Log(INFO, true, "xuid: %llu",	 playerInfo.xuid);
	Log(INFO, true, "name: %s",		 playerInfo.name);
	Log(INFO, true, "userID: %i",	 playerInfo.userID);
	Log(INFO, true, "guid: %s",		 playerInfo.guid);
	Log(INFO, true, "friendsID: %lu", playerInfo.friendsID);
	Log(INFO, true, "friendsName: %s",playerInfo.friendsName);
	Log(INFO, true, "fakeplayer: %i", playerInfo.fakeplayer);
	Log(INFO, true, "ishltv: %i",	 playerInfo.ishltv);
	Log(INFO, true, "isreplay: %i",	 playerInfo.isreplay);
	//Log(INFO, true, "customFiles: %llu",	playerInfo.customFiles);
	Log(INFO, true, "filesDownloaded: %s",	playerInfo.filesDownloaded);

	Log(INFO, true, "Check if player is banned.");
	for (const auto& i : banList)
	{
		Log(INFO, true, "username: %s", i.username.c_str());
		Log(INFO, true, "guid: %s", i.guid.c_str());
		//! For some reason this is returning false when it should be true. Will look into it later.
		if (FSubStr(playerInfo.name, i.username.c_str()) || FSubStr(playerInfo.guid, i.guid.c_str()))
		{
			const char* bannedStr = _bstr_t(g_pLocalize->FindSafe("#P2MM_BannedFromServer"));
			V_strncpy(reject, bannedStr, maxrejectlen);
			*bAllowConnect = false;
			return PLUGIN_STOP;
		}
	}

	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: Called when a player is fully connected to the server.
//			Here we check if they are on the ban list or not, and stop their connection if they are.
//			!WARNING!: The player entity has yet to be fully spawned in so manipulation to the entity is not possible.
//---------------------------------------------------------------------------------
void CP2MMServerPlugin::ClientFullyConnect(edict_t* pEntity)
{
	Log(INFO, true, "Player Joined (ClientFullyConnect)!");
	p2mm_lastmap.SetValue(CUR_MAPFILE_NAME);
}

//---------------------------------------------------------------------------------
// Purpose: Unused callbacks
//---------------------------------------------------------------------------------
#pragma region UNUSED_CALLBACKS
void CP2MMServerPlugin::Pause(void) {}
void CP2MMServerPlugin::UnPause(void) {}
void CP2MMServerPlugin::ClientDisconnect(edict_t* pEntity) {}
void CP2MMServerPlugin::ClientPutInServer(edict_t* pEntity, char const* playername) {}
void CP2MMServerPlugin::ClientSettingsChanged(edict_t* pEdict) {}
PLUGIN_RESULT CP2MMServerPlugin::NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) { return PLUGIN_CONTINUE; }
void CP2MMServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) {}
void CP2MMServerPlugin::OnEdictAllocated(edict_t* edict) {}
void CP2MMServerPlugin::OnEdictFreed(const edict_t* edict) {}
bool CP2MMServerPlugin::BNetworkCryptKeyCheckRequired(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey) { return false; }
bool CP2MMServerPlugin::BNetworkCryptKeyValidate(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte* pbEncryptedBufferFromClient, byte* pbPlainTextKeyForNetchan) { return true; }
#pragma endregion