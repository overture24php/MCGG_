// ---------------------------------------------------------------------------
// Implementasi status file. Berguna untuk diagnose tanpa logcat.
// ---------------------------------------------------------------------------
#include "status.h"
#include "il2cpp.h"
#include "config.h"
#include "log.h"

#include <cstdio>
#include <cstring>

namespace status {

// Cache nilai saat pertama kali — FindClass() memanggil API il2cpp (GC
// safepoint) yang bisa macet kalau dipanggil dari loop kita, jadi hanya satu
// kali lalu pakai cache.
static bool g_cached = false;
static bool cs_ready = false, k_shop = false, k_pd = false, k_bd = false, k_res = false;

static void CacheOnce() {
    if (g_cached) return;
    g_cached = true;
    cs_ready = il2::CsImage() != nullptr;
    k_shop = il2::FindClass("", "MCLogicHeroShop") != nullptr;
    k_pd   = il2::FindClass("", "MCChessPlayerData") != nullptr;
    k_bd   = il2::FindClass("", "MCBattleData") != nullptr;
    k_res  = il2::FindClass("MTTDProto", "Cmd_Battle_Result_CS") != nullptr;
}

void Update() {
    static int counter = 0;
    const bool tick = (++counter % 5 == 1);
    if (tick) LOGI("[STATUS] update #%d", counter);
    FILE* f = std::fopen("/sdcard/mcggmod_status.txt", "w");
    if (!f) { if (tick) LOGW("[STATUS] gagal buka status file"); return; }

    CacheOnce();   // sekali saja, bukan tiap tick

    std::fprintf(f, "=== MCGG Mod Status ===\n");
    std::fprintf(f, "il2cpp_ready: %s\n", cs_ready ? "YES" : "NO");
    std::fprintf(f, "MCLogicHeroShop: %s\n", k_shop ? "FOUND" : "MISSING");
    std::fprintf(f, "MCChessPlayerData: %s\n", k_pd ? "FOUND" : "MISSING");
    std::fprintf(f, "MCBattleData: %s\n", k_bd ? "FOUND" : "MISSING");
    std::fprintf(f, "Cmd_Battle_Result_CS: %s\n", k_res ? "FOUND" : "MISSING");
    std::fprintf(f, "\n=== Config ===\n");
    std::fprintf(f, "preclear: %d\n", cfg::t.preclear);
    std::fprintf(f, "autobuy_guin: %d\n", cfg::t.autobuy_guin);
    std::fprintf(f, "autowin_bypass: %d\n", cfg::t.autowin_bypass);
    std::fprintf(f, "autowin: %d\n", cfg::t.autowin);
    std::fprintf(f, "autostack: %d\n", cfg::t.autostack);
    std::fprintf(f, "clear_stack: %d\n", cfg::t.clear_stack);
    std::fprintf(f, "skip_guide: %d\n", cfg::t.skip_guide);
    std::fprintf(f, "\n=== Stack ===\n");
    std::fprintf(f, "count: %d/%d\n", cfg::stack_count, cfg::stack_target);

    std::fclose(f);
}

} // namespace status
