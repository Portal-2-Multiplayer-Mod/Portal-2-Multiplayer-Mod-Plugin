//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//

#include <string>

#include "globals.hpp"

inline const char* GetFormattedPrint(const char* pMsg)
{
	char buf[260];
	V_snprintf(buf, sizeof(buf), "(P2:MM Plugin): %s\n", pMsg);
	return buf;
}

//---------------------------------------------------------------------------------
// Purpose: Logging for the plugin by adding a prefix and line break. level: 0 = Msg/DevMsg, 1 = Warning/DevWarning
//---------------------------------------------------------------------------------
void P2MMLog(int level, bool dev, const char* pMsg, ...)
{


	if (!dev || !p2mm_developer.GetBool())
	{
		return;
	}

	switch (level)
	{
	case 0:
		ConColorMsg(P2MM_CONSOLE_COLOR, GetFormattedPrint(pMsg));
		break;
	case 1:
		Warning(GetFormattedPrint(pMsg));
		break;
	default:
		Warning("(P2:MM PLUGIN): P2MMLog level set outside of 0-1, \"%i\", defaulting to ConColorMsg().\n", level);
		ConColorMsg(P2MM_CONSOLE_COLOR, GetFormattedPrint(pMsg));
		break;
	}
}

//---------------------------------------------------------------------------------
// Purpose: Gets player entity index by userid.
//---------------------------------------------------------------------------------
int GetPlayerIndex(int userid)
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

// If String Equals String helper function
inline bool FStrEq(const char* sz1, const char* sz2)
{
	return (Q_stricmp(sz1, sz2) == 0);
}

// If String Has Substring helper function
inline bool FSubStr(const char* sz1, const char* search)
{
	return (Q_strstr(sz1, search));
}

// Helper functions taken from utils.h which entity to entity index and entity index to entity conversion
// Entity to entity index
inline int ENTINDEX(edict_t* pEdict)
{
	if (!pEdict)
		return 0;
	int edictIndex = pEdict - gpGlobals->pEdicts;
	Assert(edictIndex < MAX_EDICTS && edictIndex >= 0);
	return edictIndex;
}

// Entity index to entity
inline edict_t* INDEXENT(int iEdictNum)
{
	Assert(iEdictNum >= 0 && iEdictNum < MAX_EDICTS);
	if (gpGlobals->pEdicts)
	{
		edict_t* pEdict = gpGlobals->pEdicts + iEdictNum;
		if (pEdict->IsFree())
			return NULL;
		return pEdict;
	}
	return NULL;
}