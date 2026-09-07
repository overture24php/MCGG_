// Zygisk native module — inject mod menu ke game
// Compile sebagai shared library, load oleh Magisk Zygisk

#include <cstdio>
#include <cstring>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_TAG "MCGG_Zygisk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Zygisk API (sederhana — compatible dengan Magisk Delta)
extern "C" {

// Zygisk module struct
struct ZygiskModule {
    int version;
    void* onLoad;      // callback saat module di-load
    void* onAppStart;  // callback saat app dimulai
};

// Callback: saat module di-load oleh Zygisk
void zygiskOnLoad() {
    LOGI("[+] MCGG Zygisk module loaded");
}

// Callback: saat app (game) dimulai
// processName = "com.mobilechess.gp:UnityKillsMe" untuk child process
void zygiskOnAppStart(const char* processName) {
    LOGI("[+] App started: %s", processName);
    
    // Hanya inject ke child process game
    if (strstr(processName, "com.mobilechess.gp") == nullptr) return;
    
    // Tentukan library yang di-inject
    const char* libPath = "/data/adb/modules/mcgg_mod_menu/files/libmcgg_mod_menu.so";
    
    // Cek apakah file ada
    struct stat st;
    if (stat(libPath, &st) != 0) {
        LOGE("[-] Library tidak ditemukan: %s", libPath);
        return;
    }
    
    // Load library ke proses saat ini
    void* handle = dlopen(libPath, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        LOGE("[-] dlopen gagal: %s", dlerror());
        return;
    }
    
    LOGI("[+] Library berhasil di-load: %s", libPath);
}

// Export module struct
__attribute__((visibility("default")))
__attribute__((used))
static const ZygiskModule g_module = {
    .version = 1,
    .onLoad = (void*)zygiskOnLoad,
    .onAppStart = (void*)zygiskOnAppStart,
};

} // extern "C"
