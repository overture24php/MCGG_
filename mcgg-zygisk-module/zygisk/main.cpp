#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "MCGG_Zygisk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Zygisk API (compatible dengan Magisk Delta)
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static JNIEnv* g_env = nullptr;
static bool g_enable = false;

// Check if this is the target game
bool isTargetGame(jstring appDataDir) {
    if (!appDataDir) return false;
    const char* dir = g_env->GetStringUTFChars(appDataDir, nullptr);
    bool result = strstr(dir, "com.mobilechess.gp") != nullptr;
    g_env->ReleaseStringUTFChars(appDataDir, dir);
    return result;
}

class MCGGModule : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        g_env = env;
        LOGI("[+] MCGG Zygisk module loaded");
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) return;
        g_enable = isTargetGame(args->app_data_dir);
        if (g_enable) {
            LOGI("[+] Target game detected: %s", args->nice_name);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        if (!g_enable) return;
        
        // Load mod menu library in a new thread
        std::thread([]() {
            LOGI("[+] Loading mod menu...");
            
            // Wait for libil2cpp.so to be loaded
            void* il2cpp = nullptr;
            for (int i = 0; i < 60; i++) {
                il2cpp = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
                if (il2cpp) break;
                sleep(1);
            }
            
            if (!il2cpp) {
                LOGE("[-] libil2cpp.so not found");
                return;
            }
            
            // Wait for metadata to be ready
            auto domain_get = (void*(*)())dlsym(il2cpp, "il2cpp_domain_get");
            if (!domain_get) {
                LOGE("[-] il2cpp_domain_get not found");
                return;
            }
            
            for (int i = 0; i < 60; i++) {
                auto domain = domain_get();
                if (domain) {
                    LOGI("[+] IL2CPP domain ready");
                    break;
                }
                sleep(1);
            }
            
            // Load mod menu library
            const char* libPath = "/data/adb/modules/mcgg_mod_menu/files/libmcgg_mod_menu.so";
            void* handle = dlopen(libPath, RTLD_LAZY | RTLD_GLOBAL);
            if (!handle) {
                LOGE("[-] Failed to load mod menu: %s", dlerror());
                return;
            }
            
            LOGI("[+] Mod menu loaded successfully!");
        }).detach();
    }
};

REGISTER_ZYGISK_MODULE(MCGGModule)
