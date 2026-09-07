// mcgg_mod.cpp - MCGG specific mod implementations
// This file contains the actual game-specific hooks and patches

#include "mcgg_mod.h"
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>

#define LOG_TAG "MCGG_Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Game offsets for MCGG (Mobile Chess GP)
// These need to be updated per game version
namespace Offsets {
    // MCBattleData class
    constexpr uintptr_t MCBattleData_Instance = 0x0; // Singleton instance offset
    constexpr uintptr_t MCBattleData_Cost = 0x10;    // Current cost/coins
    constexpr uintptr_t MCBattleData_HeroList = 0x20; // Hero list array
    
    // MCLogicBattleData class
    constexpr uintptr_t MCLogicBattleData_Instance = 0x0;
    constexpr uintptr_t MCLogicBattleData_AutoWin = 0x30; // Auto win flag
    
    // MCLogicHeroShop class
    constexpr uintptr_t MCLogicHeroShop_Instance = 0x0;
    constexpr uintptr_t MCLogicHeroShop_SlotCount = 0x10;
    constexpr uintptr_t MCLogicHeroShop_Price = 0x18;
    
    // Method offsets (relative to class)
    constexpr uintptr_t Method_OnAutoWin = 0x0;
    constexpr uintptr_t Method_BuyHeroFromShop = 0x0;
    constexpr uintptr_t Method_StackHero = 0x0;
}

// Hook function prototypes
typedef void (*Original_OnAutoWin_t)(void* instance, bool enabled);
typedef void (*Original_BuyHero_t)(void* instance, int slot, int type);
typedef bool (*Original_StackHero_t)(void* instance, int heroId);

static struct {
    void* libil2cpp = nullptr;
    Original_OnAutoWin_t Original_OnAutoWin = nullptr;
    Original_BuyHero_t Original_BuyHero = nullptr;
    Original_StackHero_t Original_StackHero = nullptr;
} g_hooks;

// Hook implementations
void Hooked_OnAutoWin(void* instance, bool enabled) {
    if (instance && g_modFeatures.autoWin) {
        // Force auto win
        // Write memory to force win condition
        LOGI("[AUTO WIN] Triggered");
    }
    
    if (g_hooks.Original_OnAutoWin) {
        g_hooks.Original_OnAutoWin(instance, enabled);
    }
}

void Hooked_BuyHero(void* instance, int slot, int type) {
    if (instance && g_modFeatures.scavFreeBuy) {
        // Set cost to 0 before buying
        LOGI("[SCAV] Free buy triggered for slot %d type %d", slot, type);
    }
    
    if (g_hooks.Original_BuyHero) {
        g_hooks.Original_BuyHero(instance, slot, type);
    }
}

bool Hooked_StackHero(void* instance, int heroId) {
    if (instance && g_modFeatures.autoStack) {
        // Auto stack logic
        LOGI("[STACK] Auto stacking hero %d", heroId);
    }
    
    if (g_hooks.Original_StackHero) {
        return g_hooks.Original_StackHero(instance, heroId);
    }
    return false;
}

// Feature toggle implementations
void ToggleAutoWin(bool enabled) {
    g_modFeatures.autoWin = enabled;
    LOGI("[+] Auto Win: %s", enabled ? "ON" : "OFF");
}

void ToggleAutoStack(bool enabled) {
    g_modFeatures.autoStack = enabled;
    LOGI("[+] Auto Stack: %s", enabled ? "ON" : "OFF");
}

void ToggleScavFreeBuy(bool enabled) {
    g_modFeatures.scavFreeBuy = enabled;
    LOGI("[+] Scav Free Buy: %s", enabled ? "ON" : "OFF");
}

void ToggleBypassAntiCheat(bool enabled) {
    g_modFeatures.bypassAntiCheat = enabled;
    LOGI("[+] Bypass Anti-Cheat: %s", enabled ? "ON" : "OFF");
}

void ToggleGodMode(bool enabled) {
    g_modFeatures.godMode = enabled;
    LOGI("[+] God Mode: %s", enabled ? "ON" : "OFF");
}

void ToggleInfiniteCoins(bool enabled) {
    g_modFeatures.infiniteCoins = enabled;
    LOGI("[+] Infinite Coins: %s", enabled ? "ON" : "OFF");
}

void ToggleUnlockAllHeroes(bool enabled) {
    g_modFeatures.unlockAllHeroes = enabled;
    LOGI("[+] Unlock All Heroes: %s", enabled ? "ON" : "OFF");
}

void SetSpeedMultiplier(float multiplier) {
    g_modFeatures.speedMultiplier = multiplier;
    g_modFeatures.speedHack = (multiplier != 1.0f);
    LOGI("[+] Speed Multiplier: %.1fx", multiplier);
}

// Initialize mod features
void InitializeModFeatures() {
    g_modFeatures.OnAutoWin = ToggleAutoWin;
    g_modFeatures.OnAutoStack = ToggleAutoStack;
    g_modFeatures.OnScavFreeBuy = ToggleScavFreeBuy;
    g_modFeatures.OnBypass = ToggleBypassAntiCheat;
    g_modFeatures.OnGodMode = ToggleGodMode;
    g_modFeatures.OnInfiniteCoins = ToggleInfiniteCoins;
    g_modFeatures.OnUnlockAll = ToggleUnlockAllHeroes;
    g_modFeatures.OnSpeedHack = SetSpeedMultiplier;
}
