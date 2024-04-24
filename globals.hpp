//===========================================================================//
//
// Author: Orsell
// Purpose: Global functions & variables used repeatedly throughout the plugin
// 
//===========================================================================//

#include "eiface.h"

extern IVEngineServer* engine;
extern CGlobalVars* gpGlobals;

#define P2MM_CONSOLE_COLOR Color(0, 148, 100)

void P2MMLog(int level, bool dev, const tchar* pMsg, ...);
int UserIDToEntityIndex(int userid);

// useful helper func
inline bool FStrEq(const char* sz1, const char* sz2)
{
	return (Q_stricmp(sz1, sz2) == 0);
}