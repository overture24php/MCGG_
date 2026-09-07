// ---------------------------------------------------------------------------
// Entry point .so
//
// Pola diambil dari libTool (ptr_inject_ToolVIP.sh):
//   .init_array -> kode jalan otomatis begitu .so di-dlopen, tidak butuh
//   fungsi eksport bernama. JNI_OnLoad disediakan sebagai jalur alternatif.
//
// Semua kerja berat dipindah ke thread terpisah: constructor .so tidak boleh
// memblokir loader, dan metadata il2cpp baru siap SETELAH packer selesai
// (APK ini dipak: global-metadata.dat 0 byte, payload di libResources_z0_*).
// ---------------------------------------------------------------------------
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "features.h"
#include "status.h"
#include "log.h"

#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>

namespace feat {
void TriggerAutoWin();      // autowin.cpp
void ResetStackState();     // autostack.cpp
void InitAll() {
    InitAutoWin();
    InitAutoStack();
    InitPreClear();
    InitFreeBuy();
}
} // namespace feat

static void WriteStatus(const char* step) {
    FILE* f = std::fopen("/sdcard/mcggmod_status.txt", "w");
    if (f) {
        std::fprintf(f, "step: %s\n", step);
        std::fprintf(f, "time: %ld\n", (long)time(nullptr));
        std::fclose(f);
    }
}

static void* MainThread(void*) {
    LOGI("=== MCGG mod start (arm64) ===");
    WriteStatus("start");

    // 1. tunggu il2cpp + metadata benar-benar siap (packer)
    WriteStatus("waiting_for_il2cpp");
    if (!il2::Wait(120000)) {
        LOGE("il2cpp tidak siap, mod berhenti");
        WriteStatus("FAILED_il2cpp_not_ready");
        return nullptr;
    }
    WriteStatus("il2cpp_ready");

    // 2. attach thread ini ke domain sebelum menyentuh objek managed
    il2::ScopedThread st;

    // 3. gum
    if (!hook::Init()) {
        LOGE("gum gagal, mod berhenti");
        return nullptr;
    }

    // 4. toggle dari /sdcard/mcggmod.conf + watcher
    cfg::StartWatcher();

    // 5. pasang semua hook
    feat::InitAll();
    LOGI("=== semua hook terpasang ===");

    // 6. loop kecil: layani aksi sekali-pakai (autowin trigger) + update status
    bool prev_autowin = false;
    int  status_tick = 0;
    for (;;) {
        if (cfg::t.autowin && !prev_autowin) {
            feat::TriggerAutoWin();
            cfg::t.autowin = false;
        }
        prev_autowin = cfg::t.autowin;

        // update status file tiap ~1 detik
        if (++status_tick >= 2) {
            status_tick = 0;
            status::Update();
        }

        usleep(500 * 1000);
    }
    return nullptr;
}

static void StartOnce() {
    static bool started = false;
    if (started) return;
    started = true;
    pthread_t th;
    pthread_create(&th, nullptr, MainThread, nullptr);
    pthread_detach(th);
}

// jalur utama: dijalankan otomatis saat .so di-dlopen
__attribute__((constructor))
static void OnLoad() {
    StartOnce();
}

// jalur alternatif kalau di-load lewat System.loadLibrary
extern "C" __attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM*, void*) {
    StartOnce();
    return JNI_VERSION_1_6;
}
