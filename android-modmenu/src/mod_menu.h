// mod_menu.h
#ifndef MOD_MENU_H
#define MOD_MENU_H

#include "imgui.h"

struct ModFeatures {
    bool menuVisible = true;
    bool autoWin = false;
    bool autoStack = false;
    bool scavFreeBuy = false;
    bool bypassAntiCheat = false;
    bool godMode = false;
    bool infiniteCoins = false;
    bool unlockAllHeroes = false;
    bool speedHack = false;
    float speedMultiplier = 1.0f;
    
    // Callbacks
    void (*OnAutoWin)(bool) = nullptr;
    void (*OnAutoStack)(bool) = nullptr;
    void (*OnScavFreeBuy)(bool) = nullptr;
    void (*OnBypass)(bool) = nullptr;
    void (*OnGodMode)(bool) = nullptr;
    void (*OnInfiniteCoins)(bool) = nullptr;
    void (*OnUnlockAll)(bool) = nullptr;
    void (*OnSpeedHack)(float) = nullptr;
};

void RenderFeatureToggle(const char* name, bool& feature, void (*callback)(bool) = nullptr);
void RenderSliderFloat(const char* name, float& value, float min, float max, void (*callback)(float) = nullptr);
void RenderMainWindow(ModFeatures& features);

#endif // MOD_MENU_H
