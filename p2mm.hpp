//===========================================================================//
//
// Author: Nanoman2525
// Maintainer: Orsell
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#include "globals.hpp"

#define P2MM_PLUGIN_VERSION "1.1" // Update this when a new version of the plugin is released
#define P2MM_VERSION "2.3" // Update this for whatever P2:MM Version it's released with

//---------------------------------------------------------------------------------
// Purpose: Portal 2: Multiplayer Mod server plugin class
//---------------------------------------------------------------------------------

class CP2MMServerPlugin : public IServerPluginCallbacks, public IGameEventListener2
{
public:
	CP2MMServerPlugin();
	~CP2MMServerPlugin();

	// IServerPluginCallbacks methods
	virtual bool			Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory);
	virtual void			Unload(void);
	virtual void			Pause(void);
	virtual void			UnPause(void);
	virtual const char*     GetPluginDescription(void);
	virtual void			LevelInit(char const* pMapName);
	virtual void			ServerActivate(edict_t* pEdictList, int edictCount, int clientMax);
	virtual void			GameFrame(bool simulating);
	virtual void			LevelShutdown(void);
	virtual void			ClientActive(edict_t* pEntity);
	virtual void			ClientDisconnect(edict_t* pEntity);
	virtual void			ClientPutInServer(edict_t* pEntity, char const* playername);
	virtual void			SetCommandClient(int index);
	virtual void			ClientSettingsChanged(edict_t* pEdict);
	virtual PLUGIN_RESULT	ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen);
	virtual void			ClientFullyConnect(edict_t* pEntity);
	virtual PLUGIN_RESULT	ClientCommand(edict_t* pEntity, const CCommand& args);
	virtual PLUGIN_RESULT	NetworkIDValidated(const char* pszUserName, const char* pszNetworkID);
	virtual void			OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue);
	virtual void			OnEdictAllocated(edict_t* edict);
	virtual void			OnEdictFreed(const edict_t* edict);
	virtual bool			BNetworkCryptKeyCheckRequired(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, bool bClientWantsToUseCryptKey);
	virtual bool			BNetworkCryptKeyValidate(uint32 unFromIP, uint16 usFromPort, uint32 unAccountIdProvidedByClient, int nEncryptionKeyIndexFromClient, int numEncryptedBytesFromClient, byte* pbEncryptedBufferFromClient, byte* pbPlainTextKeyForNetchan);

	// IGameEventListener2 methods
	virtual void			FireGameEvent(IGameEvent* event);
	virtual int				GetEventDebugID(void);

	virtual int				GetCommandIndex() { return m_iClientCommandIndex; }

	bool		m_bSeenFirstRunPrompt;

private:

	bool		m_bPluginLoaded;
	bool		m_bNoUnload;

	int			m_iClientCommandIndex;
};

extern CP2MMServerPlugin g_P2MMServerPlugin;
