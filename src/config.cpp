// ---------------------------------------------------------------------------
// Baca toggle dari /sdcard/mcggmod.conf + thread watcher.
// Format: satu "key=0/1" per baris, baris kosong / "#" diabaikan.
// ---------------------------------------------------------------------------
#include "config.h"
#include "log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

namespace cfg {

Toggles t{};
int stack_count  = 0;
int stack_target = 14;

static const char* kPaths[] = {
    "/sdcard/mcggmod.conf",
    "/storage/emulated/0/mcggmod.conf",
    "/data/local/tmp/mcggmod.conf",
};

static void ApplyKV(const char* k, int v) {
    bool b = (v != 0);
    if      (!std::strcmp(k, "preclear"))       t.preclear = b;
    else if (!std::strcmp(k, "autobuy_guin"))   t.autobuy_guin = b;
    else if (!std::strcmp(k, "autowin_bypass")) t.autowin_bypass = b;
    else if (!std::strcmp(k, "autowin"))        t.autowin = b;
    else if (!std::strcmp(k, "autostack"))      t.autostack = b;
    else if (!std::strcmp(k, "clear_stack"))    t.clear_stack = b;
}

void Load() {
    FILE* f = nullptr;
    for (const char* p : kPaths) {
        f = std::fopen(p, "r");
        if (f) break;
    }
    if (!f) return;   // tidak ada file = pakai default, bukan error

    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char key[64] = {0};
        int  val = 0;
        if (std::sscanf(line, " %63[a-z_] = %d", key, &val) == 2)
            ApplyKV(key, val);
    }
    std::fclose(f);
}

static void* WatcherThread(void*) {
    for (;;) {
        Load();

        // clear_stack = aksi sekali pakai
        if (t.clear_stack) {
            stack_count = 0;
            t.clear_stack = false;
            LOGI("[STACK] counter direset manual -> 0/%d", stack_target);
        }
        usleep(2000 * 1000);
    }
    return nullptr;
}

void StartWatcher() {
    Load();
    LOGI("cfg: preclear=%d guin=%d bypass=%d autowin=%d stack=%d",
         t.preclear, t.autobuy_guin, t.autowin_bypass, t.autowin, t.autostack);
    pthread_t th;
    pthread_create(&th, nullptr, WatcherThread, nullptr);
    pthread_detach(th);
}

} // namespace cfg
