// mod_menu.cpp - Mod menu rendering and feature management

#include "mod_menu.h"
#include "imgui.h"
#include <android/log.h>

#define LOG_TAG "MCGG_ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

void RenderFeatureToggle(const char* name, bool& feature, void (*callback)(bool)) {
    if (ImGui::Checkbox(name, &feature)) {
        if (callback) callback(feature);
    }
}

void RenderSliderFloat(const char* name, float& value, float min, float max, void (*callback)(float)) {
    if (ImGui::SliderFloat(name, &value, min, max)) {
        if (callback) callback(value);
    }
}

void RenderMainWindow(ModFeatures& features) {
    ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("MCGG Mod Menu v1.0", &features.menuVisible);
    
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Mobile Chess GP Mod Menu");
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("Battle Features", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderFeatureToggle("Auto Win", features.autoWin, features.OnAutoWin);
        RenderFeatureToggle("Auto Stack", features.autoStack, features.OnAutoStack);
        RenderFeatureToggle("God Mode", features.godMode, features.OnGodMode);
    }
    
    if (ImGui::CollapsingHeader("Economy Features")) {
        RenderFeatureToggle("Scav Free Buy", features.scavFreeBuy, features.OnScavFreeBuy);
        RenderFeatureToggle("Infinite Coins", features.infiniteCoins, features.OnInfiniteCoins);
        RenderFeatureToggle("Unlock All Heroes", features.unlockAllHeroes, features.OnUnlockAll);
    }
    
    if (ImGui::CollapsingHeader("Exploit Features")) {
        RenderFeatureToggle("Bypass Anti-Cheat", features.bypassAntiCheat, features.OnBypass);
        RenderSliderFloat("Speed Multiplier", features.speedMultiplier, 0.1f, 10.0f, features.OnSpeedHack);
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Unload Menu", ImVec2(-1, 40))) {
        features.menuVisible = false;
    }
    
    ImGui::End();
}
