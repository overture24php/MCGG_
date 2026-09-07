#ifndef MCGG_HOOK_H
#define MCGG_HOOK_H

#include "includes/Dobby/dobby.h"

// Macros untuk hook
#define HOOKAF(ret, func, ...) \
    ret (*orig##func)(__VA_ARGS__); \
    ret Hook##func(__VA_ARGS__)

#define HOOK(addr, func, orig) \
    DobbyHook((void*)(addr), (void*)Hook##func, (void**)&orig)

// Hook function declarations
HOOKAF(void, visit, void* t, void* bOpt);
HOOKAF(void, Refresh, void* isAutoRefresh, void* refreshCost, void* playerLv, void* type);
HOOKAF(void, CheckFreeBuy, void* slotHeroes);
HOOKAF(void, CraftReserve, void* accountId, void* heroGuidList, void* slotIndex, void* newHeroGuid, void* newHeroId, void* newStarLevel);
HOOKAF(void, CraftField, void* accountId, void* heroGuidList, void* newHeroGuid, void* newHeroId, void* newStarLevel);

#endif
