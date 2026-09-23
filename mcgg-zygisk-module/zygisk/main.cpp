#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <android/log.h>

#define LOG_TAG "MCGG_Zygisk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Zygisk API — zygisk.hpp BUNDLED di folder ini (tidak perlu download).
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static JNIEnv* g_env = nullptr;
static bool g_enable = false;

// args->nice_name / app_data_dir adalah jstring — JANGAN di-print sebagai %s.
static bool jstrContains(jstring s, const char* needle) {
    if (!s || !g_env) return false;
    const char* c = g_env->GetStringUTFChars(s, nullptr);
    if (!c) return false;
    bool r = strstr(c, needle) != nullptr;
    g_env->ReleaseStringUTFChars(s, c);
    return r;
}

// Payload = libmcggmod.so (IL2CPP hook; tunggu metadata SELENGKAP di dalem).
static const char* kPayload = "/data/adb/modules/mcgg_mod_menu/files/libmcggmod.so";

// JALAN DI PROSES ANAK (post-fork, setelah app specialized — disinilah
// libil2cpp.so nanti dimuat; di zygote/induk belum ada).
// Tunggu libil2cpp.so benar-benar sudah di-dlopen proses ini SEBELUM payload
// dipasang; payload sendiri polling metadata (il2::Wait) sampai siap.
static void* loaderThread(void*) {
    LOGI("[+] loader: tunggu libil2cpp.so (proses anak, pid=%d)", (int)getpid());
    void* il = nullptr;
    for (int i = 0; i < 120 && !il; i++) {
        // RTLD_NOLOAD: hanya cek keberadaan, TIDAK memaksa load
        il = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!il) sleep(1);
    }
    if (!il) { LOGE("[-] libil2cpp.so tidak muncul dalam 120s"); return nullptr; }
    LOGI("[+] libil2cpp.so termuat");

    for (int i = 0; i < 30 && access(kPayload, R_OK) != 0; i++) sleep(1);
    void* h = dlopen(kPayload, RTLD_LAZY | RTLD_GLOBAL);
    if (!h) { LOGE("[-] dlopen payload gagal: %s", dlerror()); return nullptr; }
    LOGI("[+] payload libmcggmod.so loaded");
    return nullptr;
}

class MCGGModule : public zygisk::ModuleBase {
public:
    void onLoad(Api*, JNIEnv* env) override {
        g_env = env;
        LOGI("[+] MCGG zygisk module loaded");
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        g_enable = false;
        if (!args) return;
        if (jstrContains(args->nice_name, "com.mobilechess.gp") ||
            jstrContains(args->app_data_dir, "com.mobilechess.gp")) {
            g_enable = true;
            LOGI("[+] target game terdeteksi (uid=%d)", (int)args->uid);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        if (!g_enable) return;
        pthread_t th;
        pthread_create(&th, nullptr, loaderThread, nullptr);
        pthread_detach(th);
    }
};

REGISTER_ZYGISK_MODULE(MCGGModule)
