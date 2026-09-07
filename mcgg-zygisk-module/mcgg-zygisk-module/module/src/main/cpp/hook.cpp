#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include <EGL/egl.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "Includes/Dobby/dobby.h"
#include "Include/Unity.h"
#include "Misc.h"
#include "hook.h"
#include <iostream>
#include "include/il2cpp.h"
#include "menu.h"
#include "functions.h"

#define GamePackageName "com.mobilechess.gp"

int glHeight, glWidth;
int isGame(JNIEnv *env, jstring appDataDir) {
    if (!appDataDir) return 0;
    const char *app_data_dir = env->GetStringUTFChars(appDataDir, nullptr);
    int user = 0;
    static char package_name[256];
    if (sscanf(app_data_dir, "/data/%*[^/]/%d/%s", &user, package_name) != 2) {
        if (sscanf(app_data_dir, "/data/%*[^/]/%s", package_name) != 1) {
            package_name[0] = '\0';
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    }
    env->ReleaseStringUTFChars(appDataDir, app_data_dir);
    return 0;
}

bool setupimg;

HOOKAF(void, Input, void *thiz, void *ex_ab, void *ex_ac) {
    origInput(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
    return;
}

HOOKAF(int32_t, Consume, void *thiz, void *arg1, bool arg2, long arg3, uint32_t *arg4, AInputEvent **input_event) {
    auto result = origConsume(thiz, arg1, arg2, arg3, arg4, input_event);
    if(result != 0 || *input_event == nullptr) return result;
    ImGui_ImplAndroid_HandleInputEvent(*input_event);
    return result;
}

void *hack_thread(void *arg) {
    // Step 1: Wait for libil2cpp.so to be loaded
    do {
        sleep(1);
        g_il2cppBaseMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
    } while (!g_il2cppBaseMap.isValid());
    KITTY_LOGI("il2cpp base: %p", (void*)(g_il2cppBaseMap.startAddress));

    // Step 2: Wait for metadata to be ready (game dipak)
    // Poll il2cpp_domain_get() until it returns non-null AND Assembly-CSharp.dll is found
    bool metadata_ready = false;
    int attempts = 0;
    while (!metadata_ready && attempts < 60) {
        il2cpp_init = (il2cpp_init_t)dlsym(g_il2cppBaseMap.startAddress, "il2cpp_init");
        il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(g_il2cppBaseMap.startAddress, "il2cpp_domain_get");
        
        if (il2cpp_domain_get) {
            auto domain = il2cpp_domain_get();
            if (domain) {
                size_t n = 0;
                auto assemblies = il2cpp_domain_get_assemblies(domain, &n);
                if (assemblies && n > 0) {
                    metadata_ready = true;
                    KITTY_LOGI("Metadata ready after %d attempts (%zu assemblies)", attempts, n);
                }
            }
        }
        if (!metadata_ready) {
            sleep(1);
            attempts++;
        }
    }

    // Step 3: Initialize IL2CPP by-name functions
    if (metadata_ready) {
        IL2CPP::Init(g_il2cppBaseMap.startAddress);
    }

    // Step 4: Get pointers and hooks
    Pointers();
    Hooks();

    // Step 5: Hook eglSwapBuffers for menu rendering
    auto eglhandle = dlopen("libEGL.so", RTLD_LAZY);
    auto eglSwapBuffers = dlsym(eglhandle, "eglSwapBuffers");
    
    if (eglSwapBuffers) {
        DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
        KITTY_LOGI("eglSwapBuffers hooked");
    } else {
        KITTY_LOGE("eglSwapBuffers not found");
    }

    // Step 6: Hook input for touch
    void *sym_input = DobbySymbolResolver("/system/lib/libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    if (NULL != sym_input) {
        DobbyHook(sym_input, (void*)myInput, (void**)&origInput);
    } else {
        sym_input = DobbySymbolResolver("/system/lib/libinput.so", "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE");
        if(NULL != sym_input) {
            DobbyHook(sym_input, (void*)myConsume, (void**)&origConsume);
        }
    }

    LOGI("Draw Done!");
    return nullptr;
}
