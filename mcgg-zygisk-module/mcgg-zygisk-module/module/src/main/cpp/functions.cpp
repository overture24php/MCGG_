#include "functions.h"
#include "hook.h"
#include "menu.h"
#include <cstring>

Toggles g_toggles;
int g_stack_count = 0;
int g_stack_target = 14;

size_t off_price = 0;
size_t off_slotHero = 0;
size_t off_battleTime = 0;
size_t off_invalid = 0;
size_t off_hookFlag = 0;
size_t off_hookerList = 0;

void* fn_getSlotItem = nullptr;
void* fn_getItemInfo = nullptr;
void* fn_shopLocked = nullptr;
void* fn_sendOper = nullptr;
void* fn_checkFreeBuy = nullptr;
void* fn_getFreeType = nullptr;
void* fn_onAutoWin = nullptr;
void* fn_craftReserve = nullptr;
void* fn_craftField = nullptr;

void Pointers() {
    // Resolve class pointers
    auto heroShop = IL2CPP::FindClass("", "MCLogicHeroShop");
    auto playerData = IL2CPP::FindClass("", "MCChessPlayerData");
    auto battleData = IL2CPP::FindClass("", "MCBattleData");
    auto resultCS = IL2CPP::FindClass("MTTDProto", "Cmd_Battle_Result_CS");
    auto bridge = IL2CPP::FindClass("", "BattleReceiveMessageBridge");
    auto reserve = IL2CPP::FindClass("Battle", "MCLogicReserveComp");

    // Resolve field offsets
    if (heroShop) {
        off_price = IL2CPP::GetFieldOffset(heroShop, "m_iPrice");
        off_slotHero = IL2CPP::GetFieldOffset(heroShop, "m_iHeroOrItemId");
    }
    if (resultCS) {
        off_battleTime = IL2CPP::GetFieldOffset(resultCS, "iBattleTime");
        off_invalid = IL2CPP::GetFieldOffset(resultCS, "iIsInvalidBattle");
        off_hookFlag = IL2CPP::GetFieldOffset(resultCS, "iWarmBattleHook");
        off_hookerList = IL2CPP::GetFieldOffset(resultCS, "vHooker");
    }

    // Resolve method pointers
    if (heroShop) {
        fn_getSlotItem = IL2CPP::GetMethod(heroShop, "GetSlotItem", 1);
        fn_getItemInfo = IL2CPP::GetMethod(heroShop, "GetItemInfo", 1);
        fn_shopLocked = IL2CPP::GetMethod(heroShop, "GetShopLockStatus", 0);
    }
    if (playerData) {
        fn_checkFreeBuy = IL2CPP::GetMethod(playerData, "CheckFreeBuyHero", 1);
        fn_getFreeType = IL2CPP::GetMethod(playerData, "GetFreeBuyHeroType", 1);
    }
    if (battleData) {
        fn_onAutoWin = IL2CPP::GetMethod(battleData, "OnAutoWin", 1);
        fn_craftReserve = IL2CPP::GetMethod(battleData, "IShowHandler_CraftHeroAtReserve", -1);
        fn_craftField = IL2CPP::GetMethod(battleData, "IShowHandler_CraftHeroAtBattleField", -1);
    }
    if (bridge) {
        fn_sendOper = IL2CPP::GetMethod(bridge, "SendBattleOperData", 4);
    }
}

void Hooks() {
    // Hook Cmd_Battle_Result_CS.visit untuk bypass invalid
    auto resultCS = IL2CPP::FindClass("MTTDProto", "Cmd_Battle_Result_CS");
    if (resultCS) {
        auto visit = IL2CPP::GetMethod(resultCS, "visit", 2);
        if (visit) {
            DobbyHook(visit, (void*)Hook_visit, (void**)&orig_visit);
        }
    }

    // Hook MCLogicHeroShop.Refresh untuk pre-clear
    auto heroShop = IL2CPP::FindClass("", "MCLogicHeroShop");
    if (heroShop) {
        auto refresh = IL2CPP::GetMethod(heroShop, "Refresh", 4);
        if (refresh) {
            DobbyHook(refresh, (void*)Hook_Refresh, (void**)&orig_Refresh);
        }
    }

    // Hook MCChessPlayerData.CheckFreeBuyHero untuk auto buy
    auto playerData = IL2CPP::FindClass("", "MCChessPlayerData");
    if (playerData) {
        auto checkFree = IL2CPP::GetMethod(playerData, "CheckFreeBuyHero", 1);
        if (checkFree) {
            DobbyHook(checkFree, (void*)Hook_CheckFreeBuy, (void**)&orig_CheckFreeBuy);
        }
    }

    // Hook MCBattleData.IShowHandler_CraftHeroAtReserve untuk auto stack
    auto battleData = IL2CPP::FindClass("", "MCBattleData");
    if (battleData) {
        auto craftR = IL2CPP::GetMethod(battleData, "IShowHandler_CraftHeroAtReserve", -1);
        if (craftR) {
            DobbyHook(craftR, (void*)Hook_CraftReserve, (void**)&orig_CraftReserve);
        }
        auto craftF = IL2CPP::GetMethod(battleData, "IShowHandler_CraftHeroAtBattleField", -1);
        if (craftF) {
            DobbyHook(craftF, (void*)Hook_CraftField, (void**)&orig_CraftField);
        }
    }
}

void Patches() {
    // Tidak ada memory patch yang diperlukan untuk fitur ini
    // Semua fitur menggunakan hook method, bukan patch
}
