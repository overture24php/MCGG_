// main.cpp - Entry point for JsHook compatibility
// Loads libmcggmod.so and shows ImGui toggle menu

#include <jni.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#define LOG_TAG "MCGG_ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VALOBALS__)

// Mod feature flags
struct ModFeatures {
    bool autoWin = false;
    bool autoStack = false;
    bool scavFreeBuy = false;
    bool bypassAntiCheat = false;
    bool godMode = false;
    bool infiniteCoins = false;
    bool unlockAllHeroes = false;
    bool speedHack = false;
    float speedMultiplier = 1.0f;
};

static ModFeatures g_features;
static bool g_menuVisible = true;
static bool g_initialized = false;

// Function pointers from libmcggmod.so
typedef void (*OnAutoWin_t)(bool enabled);
typedef void (*OnAutoStack_t)(bool enabled);
typedef void (*OnScavFreeBuy_t)(bool enabled);
typedef void (*OnBypass_t)(bool enabled);
typedef void (*OnGodMode_t)(bool enabled);
typedef void (*OnInfiniteCoins_t)(bool enabled);
typedef void (*OnUnlockAll_t)(bool enabled);
typedef void (*OnSpeedHack_t)(float multiplier);

static struct {
    void* handle = nullptr;
    OnAutoWin_t OnAutoWin = nullptr;
    OnAutoStack_t OnAutoStack = nullptr;
    OnScavFreeBuy_t OnScavFreeBuy = nullptr;
    OnBypass_t OnBypass = nullptr;
    OnGodMode_t OnGodMode = nullptr;
    OnInfiniteCoins_t OnInfiniteCoins = nullptr;
    OnUnlockAll_t OnUnlockAll = nullptr;
    OnSpeedHack_t OnSpeedHack = nullptr;
} g_modLib;

// Forward declarations
void InitializeImGui();
void DrawModMenu();
void CleanupImGui();
void LoadModLibrary();
void UnloadModLibrary();

// Load the actual mod library (libmcggmod.so)
void LoadModLibrary() {
    const char* libPath = "/data/local/tmp/libmcggmod.so";
    
    // Try multiple paths
    const char* paths[] = {
        "/data/local/tmp/libmcggmod.so",
        "/sdcard/Android/data/com.mobilechess.gp/files/libmcggmod.so",
        "/data/user/0/com.mobilechess.gp/files/libmcggmod.so",
        nullptr
    };
    
    for (int i = 0; paths[i] != nullptr; i++) {
        g_modLib.handle = dlopen(paths[i], RTLD_NOW | RTLD_GLOBAL);
        if (g_modLib.handle) {
            LOGI("[+] Loaded mod library from: %s", paths[i]);
            break;
        }
    }
    
    if (!g_modLib.handle) {
        LOGE("[!] Failed to load mod library: %s", dlerror());
        return;
    }
    
    // Load function symbols
    g_modLib.OnAutoWin = (OnAutoWin_t)dlsym(g_modLib.handle, "OnAutoWin");
    g_modLib.OnAutoStack = (OnAutoStack_t)dlsym(g_modLib.handle, "OnAutoStack");
    g_modLib.OnScavFreeBuy = (OnScavFreeBuy_t)dlsym(g_modLib.handle, "OnScavFreeBuy");
    g_modLib.OnBypass = (OnBypass_t)dlsym(g_modLib.handle, "OnBypass");
    g_modLib.OnGodMode = (OnGodMode_t)dlsym(g_modLib.handle, "OnGodMode");
    g_modLib.OnInfiniteCoins = (OnInfiniteCoins_t)dlsym(g_modLib.handle, "OnInfiniteCoins");
    g_modLib.OnUnlockAll = (OnUnlockAll_t)dlsym(g_modLib.handle, "OnUnlockAll");
    g_modLib.OnSpeedHack = (OnSpeedHack_t)dlsym(g_modLib.handle, "OnSpeedHack");
    
    LOGI("[+] Mod library loaded successfully");
}

void UnloadModLibrary() {
    if (g_modLib.handle) {
        dlclose(g_modLib.handle);
        g_modLib.handle = nullptr;
    }
}

// Initialize ImGui for rendering
void InitializeImGui() {
    if (g_initialized) return;
    
    LOGI("[+] Initializing ImGui...");
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Load font
    io.Fonts->AddFontFromFileTTF("/system/fonts/Roboto-Regular.ttf", 18.0f);
    
    // Setup style
    ImGui::StyleColorsDark();
    // Note: styling applied via ImGui::GetStyle() when needed
    
    // Initialize backends
    // Note: Android surface initialization happens in onSurfaceCreated
    // This is just the context setup
    
    g_initialized = true;
    LOGI("[+] ImGui initialized");
}

void DrawModMenu() {
    if (!g_menuVisible) return;
    
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("MCGG Mod Menu", &g_menuVisible);
    
    ImGui::Text("Features:");
    ImGui::Separator();
    
    // Feature toggles
    if (ImGui::Checkbox("Auto Win", &g_features.autoWin)) {
        if (g_modLib.OnAutoWin) g_modLib.OnAutoWin(g_features.autoWin);
    }
    
    if (ImGui::Checkbox("Auto Stack", &g_features.autoStack)) {
        if (g_modLib.OnAutoStack) g_modLib.OnAutoStack(g_features.autoStack);
    }
    
    if (ImGui::Checkbox("Scav Free Buy", &g_features.scavFreeBuy)) {
        if (g_modLib.OnScavFreeBuy) g_modLib.OnScavFreeBuy(g_features.scavFreeBuy);
    }
    
    if (ImGui::Checkbox("Bypass Anti-Cheat", &g_features.bypassAntiCheat)) {
        if (g_modLib.OnBypass) g_modLib.OnBypass(g_features.bypassAntiCheat);
    }
    
    if (ImGui::Checkbox("God Mode", &g_features.godMode)) {
        if (g_modLib.OnGodMode) g_modLib.OnGodMode(g_features.godMode);
    }
    
    if (ImGui::Checkbox("Infinite Coins", &g_features.infiniteCoins)) {
        if (g_modLib.OnInfiniteCoins) g_modLib.OnInfiniteCoins(g_features.infiniteCoins);
    }
    
    if (ImGui::Checkbox("Unlock All Heroes", &g_features.unlockAllHeroes)) {
        if (g_modLib.OnUnlockAll) g_modLib.OnUnlockAll(g_features.unlockAllHeroes);
    }
    
    // Speed hack slider
    if (ImGui::SliderFloat("Speed Multiplier", &g_features.speedMultiplier, 0.5f, 5.0f)) {
        if (g_modLib.OnSpeedHack) g_modLib.OnSpeedHack(g_features.speedMultiplier);
    }
    
    ImGui::Separator();
    if (ImGui::Button("Unload Menu")) {
        g_menuVisible = false;
    }
    
    ImGui::End();
}

void CleanupImGui() {
    if (!g_initialized) return;
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
    
    g_initialized = false;
    LOGI("[+] ImGui cleanup complete");
}

// JNI functions called by JsHook
extern "C" {

// Main entry point called by JsHook
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[+] MCGG Mod Menu loading...");
    
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("[!] Failed to get JNI environment");
        return JNI_ERR;
    }
    
    // Load the actual mod library
    LoadModLibrary();
    
    // Initialize ImGui (will be fully initialized when surface is created)
    InitializeImGui();
    
    LOGI("[+] MCGG Mod Menu loaded successfully");
    return JNI_VERSION_1_6;
}

// Called by JsHook when surface is created
JNIEXPORT void JNICALL Java_imgui_il2cpp_tool_NativeMethods_onSurfaceCreate(JNIEnv* env, jobject thiz, jobject surface, jint width, jint height) {
    LOGI("[+] Surface created: %dx%d", width, height);
    
    // Initialize ImGui Android backend here when surface is available
    // ImGui_ImplAndroid_Init(surface);
    // ImGui_ImplOpenGL3_Init("#version 300 es");
}

// Called by JsHook when surface changes size
JNIEXPORT void JNICALL Java_imgui_il2cpp_tool_NativeMethods_onSurfaceChanged(JNIEnv* env, jobject thiz, jint width, jint height) {
    // Handle surface resize
    LOGI("[+] Surface changed: %dx%d", width, height);
}

// Called by JsHook every frame
JNIEXPORT void JNICALL Java_imgui_il2cpp_tool_NativeMethods_onDrawFrame(JNIEnv* env, jobject thiz) {
    if (!g_initialized) return;
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    
    DrawModMenu();
    
    ImGui::Render();
    
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Toggle menu visibility
JNIEXPORT void JNICALL Java_imgui_il2cpp_tool_NativeMethods_toggleMenu(JNIEnv* env, jobject thiz) {
    g_menuVisible = !g_menuVisible;
}

} // extern "C"
