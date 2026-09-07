#ifndef MCGG_FUNCTIONS_H
#define MCGG_FUNCTIONS_H

#include "include/il2cpp.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "Includes/Dobby/dobby.h"
#include "Misc.h"

// Toggle variables
struct Toggles {
    bool preclear = false;
    bool autobuy_guin = false;
    bool autowin_bypass = true;
    bool autowin = false;
    bool autostack = false;
    bool clear_stack = false;
};

extern Toggles g_toggles;
extern int g_stack_count;
extern int g_stack_target;

// Field offsets (resolved at runtime)
extern size_t off_price;
extern size_t off_slotHero;
extern size_t off_battleTime;
extern size_t off_invalid;
extern size_t off_hookFlag;
extern size_t off_hookerList;

// Method pointers
extern void* fn_getSlotItem;
extern void* fn_getItemInfo;
extern void* fn_shopLocked;
extern void* fn_sendOper;
extern void* fn_checkFreeBuy;
extern void* fn_getFreeType;
extern void* fn_onAutoWin;
extern void* fn_craftReserve;
extern void* fn_craftField;

// Pointers: resolve addresses
void Pointers();

// Hooks: install hooks
void Hooks();

// Patches: apply memory patches
void Patches();

#endif
