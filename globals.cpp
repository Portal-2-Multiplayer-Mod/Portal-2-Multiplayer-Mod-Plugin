//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//

#include <string>

#include "globals.hpp"

//---------------------------------------------------------------------------------
// Purpose: Logging for the plugin by adding a prefix and line break. level: 0 = Msg/DevMsg, 1 = Warning/DevWarning
//---------------------------------------------------------------------------------
void P2MMLog(int level, bool dev, const tchar* pMsg, ...)
{
	std::string completeMsg = ("(P2:MM PLUGIN): " + std::string(pMsg) + "\n");

	if (dev && !p2mm_developer.GetBool())
	{
		return;
	}

	switch (level)
	{
	case 0:
		Msg((completeMsg).c_str());
		break;
	case 1:
		Warning((completeMsg).c_str());
		break;
	default:
		Warning("(P2:MM PLUGIN): P2MMLog level set outside of 0-1, \"%d\", defaulting to Msg().\n", level);
		Msg((completeMsg).c_str());
		break;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Use the player's userid to get their entity index.
//---------------------------------------------------------------------------------
int UserIDToEntityIndex(int userid)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* pEdict = NULL;
		if (i >= 0 && i < gpGlobals->maxEntities)
		{
			pEdict = (edict_t*)(gpGlobals->pEdicts + i);
		}

		if (engineServer->GetPlayerUserId(pEdict) == userid)
		{
			return i;
		}
	}
	return NULL; // Return NULL if the index can't be found
}