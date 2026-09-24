// ---------------------------------------------------------------------------
// Baca toggle dari /sdcard/mcggmod.conf + thread watcher.
// Format: satu "key=0/1" per baris, baris kosong / "#" diabaikan.
//
// EDGE TRIGGER one-shot (autowin / clear_stack / skip_guide):
//   Payload menulis balik nilai one-shot ke 0 (Save()) setelah dieksekusi,
//   dan Load() hanya menyalakan one-shot saat nilai file berubah 0 -> 1.
//   Tanpa ini, watcher me-load "autowin=1" terus tiap 2 detik dan trigger
//   OnAutoWin berulang-ulang (bug re-trigger).
// Load pertama MENYERAP keadaan file (sisa "1" dari sesi lama tidak dijalankan).
// ---------------------------------------------------------------------------
#include "config.h"
#include "feature_api.h"
#include "log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
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

// nilai one-shot terakhir yang terbaca/ditulis (edge detection)
static int  prev_autowin     = 0;
static int  prev_clear_stack = 0;
static int  prev_skip_guide  = 0;
static bool first_load       = true;

// bit lock persistent per key (0=preclear 1=autobuy_guin 2=autowin_bypass
// 3=autostack). Setelah menu in-process menulis key lewat SetPersist, Load()
// tidak boleh menimpa key itu dari file (file mungkin tidak bisa ditulis /
// berisi nilai lama) — cegah toggle "pantul" balik ke nilai lama.
static uint32_t g_locked = 0;

static pthread_mutex_t g_file_mu = PTHREAD_MUTEX_INITIALIZER;

// bit index key persistent, -1 = bukan key persistent
static int KeyBit(const char* k) {
    if (!std::strcmp(k, "preclear"))       return 0;
    if (!std::strcmp(k, "autobuy_guin"))   return 1;
    if (!std::strcmp(k, "autowin_bypass")) return 2;
    if (!std::strcmp(k, "autostack"))      return 3;
    return -1;
}

// hasil parse satu kali baca file; -1 = key tidak disebut di file
struct FileVals {
    int preclear       = -1;
    int autobuy_guin   = -1;
    int autowin_bypass = -1;
    int autowin        = -1;
    int autostack      = -1;
    int clear_stack    = -1;
    int skip_guide     = -1;

    // parse file pertama yang terbuka; return false kalau tidak ada file.
    // TIDAK menyentuh `t` — murni baca untuk Load()/Save().
    bool ParseFromDisk() {
        FILE* f = nullptr;
        for (const char* p : kPaths) {
            f = std::fopen(p, "r");
            if (f) break;
        }
        if (!f) return false;
        char line[128];
        while (std::fgets(line, sizeof(line), f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char key[64] = {0};
            int  val = 0;
            if (std::sscanf(line, " %63[a-z_] = %d", key, &val) != 2) continue;
            int b = val ? 1 : 0;
            if      (!std::strcmp(key, "preclear"))       preclear = b;
            else if (!std::strcmp(key, "autobuy_guin"))   autobuy_guin = b;
            else if (!std::strcmp(key, "autowin_bypass")) autowin_bypass = b;
            else if (!std::strcmp(key, "autowin"))        autowin = b;
            else if (!std::strcmp(key, "autostack"))      autostack = b;
            else if (!std::strcmp(key, "clear_stack"))    clear_stack = b;
            else if (!std::strcmp(key, "skip_guide"))     skip_guide = b;
        }
        std::fclose(f);
        return true;
    }
};

void Load() {
    pthread_mutex_lock(&g_file_mu);
    FileVals fv;
    if (!fv.ParseFromDisk()) {
        pthread_mutex_unlock(&g_file_mu);
        return;   // tak ada file = pakai default, bukan error
    }

    // persistent: nilai file = keadaan (hanya key yang tidak di-lock menu &
    // tidak disebut file)
    if (fv.preclear       >= 0 && !(g_locked & 1u)) t.preclear       = fv.preclear != 0;
    if (fv.autobuy_guin   >= 0 && !(g_locked & 2u)) t.autobuy_guin   = fv.autobuy_guin != 0;
    if (fv.autowin_bypass >= 0 && !(g_locked & 4u)) t.autowin_bypass = fv.autowin_bypass != 0;
    if (fv.autostack      >= 0 && !(g_locked & 8u)) t.autostack      = fv.autostack != 0;

    // one-shot: fire hanya saat file berubah 0 -> 1. Load pertama menyerap
    // (first_load) supaya sisa "1" lama tidak langsung dieksekusi.
    if (fv.autowin >= 0) {
        if (!first_load && fv.autowin == 1 && prev_autowin == 0) t.autowin = true;
        prev_autowin = fv.autowin;
    }
    if (fv.clear_stack >= 0) {
        if (!first_load && fv.clear_stack == 1 && prev_clear_stack == 0) t.clear_stack = true;
        prev_clear_stack = fv.clear_stack;
    }
    if (fv.skip_guide >= 0) {
        if (!first_load && fv.skip_guide == 1 && prev_skip_guide == 0) t.skip_guide = true;
        prev_skip_guide = fv.skip_guide;
    }
    first_load = false;
    pthread_mutex_unlock(&g_file_mu);
}

// Tulis file: read-modify-write. Persistent diambil dari ISI FILE TERBARU
// (bukan dari memori `t`) supaya edit user yang belum sempat ter-load tidak
// tertimpa; hanya 3 key one-shot yang dipaksa 0 setelah dieksekusi.
// File tidak ada -> tulis default dari `t`.
void Save() {
    pthread_mutex_lock(&g_file_mu);

    // mulai dari memori, lalu override dengan isi file bila ada & key tidak
    // di-lock menu in-process (locked = memori sudah jadi sumber kebenaran)
    int v_pre = t.preclear, v_guin = t.autobuy_guin, v_byp = t.autowin_bypass,
        v_ast = t.autostack;
    FileVals fv;
    if (fv.ParseFromDisk()) {
        if (fv.preclear       >= 0 && !(g_locked & 1u)) v_pre = fv.preclear;
        if (fv.autobuy_guin   >= 0 && !(g_locked & 2u)) v_guin = fv.autobuy_guin;
        if (fv.autowin_bypass >= 0 && !(g_locked & 4u)) v_byp = fv.autowin_bypass;
        if (fv.autostack      >= 0 && !(g_locked & 8u)) v_ast = fv.autostack;
    }

    FILE* f = nullptr;
    for (const char* p : kPaths) {
        f = std::fopen(p, "w");
        if (!f) continue;
        std::fprintf(f, "preclear=%d\n",        v_pre);
        std::fprintf(f, "autobuy_guin=%d\n",    v_guin);
        std::fprintf(f, "autowin_bypass=%d\n",  v_byp);
        std::fprintf(f, "autowin=0\n");
        std::fprintf(f, "autostack=%d\n",       v_ast);
        std::fprintf(f, "clear_stack=0\n");
        std::fprintf(f, "skip_guide=0\n");
        std::fclose(f);
        break;  // path pertama yang berhasil ditulis cukup
    }
    // one-shot selalu tertulis 0 -> sinkronkan edge counter
    prev_autowin     = 0;
    prev_clear_stack = 0;
    prev_skip_guide  = 0;
    pthread_mutex_unlock(&g_file_mu);
}

static void* WatcherThread(void*) {
    for (;;) {
        Load();

        // clear_stack = aksi sekali pakai -> reset LENGKAP (count + seen GUID)
        if (t.clear_stack) {
            feat::ResetStackState();
            t.clear_stack = false;
            Save();   // tulis balik 0 ke file (edge sudah ditangani Load)
            LOGI("[STACK] counter direset manual -> 0/%d", stack_target);
        }
        usleep(2000 * 1000);
    }
    return nullptr;
}

void StartWatcher() {
    Load();
    Save();  // paksa one-shot di file ke 0 (serap sisa "1" lama tanpa eksekusi)
    LOGI("cfg: preclear=%d guin=%d bypass=%d autowin=%d stack=%d skip=%d",
         t.preclear, t.autobuy_guin, t.autowin_bypass, t.autowin, t.autostack,
         t.skip_guide);
    pthread_t th;
    pthread_create(&th, nullptr, WatcherThread, nullptr);
    pthread_detach(th);
}

// ---------------------------------------------------------------------------
// Bridge menu in-process (JNI dari MCGG.java di APK repack). Panggilan dari
// UI thread game — mutex wajib untuk akses file; field bool sendiri benign.
// ---------------------------------------------------------------------------
void SetPersist(const char* key, bool on) {
    int bit = KeyBit(key);
    if (bit < 0) return;

    pthread_mutex_lock(&g_file_mu);
    switch (bit) {
        case 0: t.preclear       = on; break;
        case 1: t.autobuy_guin   = on; break;
        case 2: t.autowin_bypass = on; break;
        case 3: t.autostack      = on; break;
    }
    g_locked |= (1u << bit);   // Load() tidak boleh timpa key ini lagi

    // tulis file (read-modify-write): persistent dari memori `t` (sumber
    // kebenaran utk key locked), one-shot pertahankan isi file (aman dikunci
    // 0 oleh Save/Load). Gagal tulis = tak apa-apa, memori sudah benar.
    FileVals fv;
    bool had = fv.ParseFromDisk();
    int aw = (had && fv.autowin    >= 0) ? fv.autowin    : 0;
    int cs = (had && fv.clear_stack >= 0) ? fv.clear_stack : 0;
    int sg = (had && fv.skip_guide >= 0) ? fv.skip_guide  : 0;
    FILE* f = nullptr;
    for (const char* p : kPaths) {
        f = std::fopen(p, "w");
        if (f) break;
    }
    if (f) {
        std::fprintf(f, "preclear=%d\n",        (int)t.preclear);
        std::fprintf(f, "autobuy_guin=%d\n",    (int)t.autobuy_guin);
        std::fprintf(f, "autowin_bypass=%d\n",  (int)t.autowin_bypass);
        std::fprintf(f, "autowin=%d\n",         aw);
        std::fprintf(f, "autostack=%d\n",       (int)t.autostack);
        std::fprintf(f, "clear_stack=%d\n",     cs);
        std::fprintf(f, "skip_guide=%d\n",      sg);
        std::fclose(f);
    }
    pthread_mutex_unlock(&g_file_mu);
    LOGI("[MENU] persist %s=%d (locked=0x%x)", key, (int)on, g_locked);
}

bool GetPersist(const char* key) {
    switch (KeyBit(key)) {
        case 0: return t.preclear;
        case 1: return t.autobuy_guin;
        case 2: return t.autowin_bypass;
        case 3: return t.autostack;
        default: return false;
    }
}

void TriggerOnce(const char* key) {
    if      (!std::strcmp(key, "autowin"))     t.autowin     = true;
    else if (!std::strcmp(key, "clear_stack")) t.clear_stack = true;
    else if (!std::strcmp(key, "skip_guide"))  t.skip_guide  = true;
    else return;
    LOGI("[MENU] trigger %s", key);
}

} // namespace cfg
