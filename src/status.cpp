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

void Update() {
    FILE* f = std::fopen("/sdcard/mcggmod_status.txt", "w");
    if (!f) return;

    std::fprintf(f, "=== MCGG Mod Status ===\n");
    std::fprintf(f, "il2cpp_ready: %s\n", il2::CsImage() ? "YES" : "NO");
    std::fprintf(f, "MCLogicHeroShop: %s\n", il2::FindClass("", "MCLogicHeroShop") ? "FOUND" : "MISSING");
    std::fprintf(f, "MCChessPlayerData: %s\n", il2::FindClass("", "MCChessPlayerData") ? "FOUND" : "MISSING");
    std::fprintf(f, "MCBattleData: %s\n", il2::FindClass("", "MCBattleData") ? "FOUND" : "MISSING");
    std::fprintf(f, "Cmd_Battle_Result_CS: %s\n", il2::FindClass("MTTDProto", "Cmd_Battle_Result_CS") ? "FOUND" : "MISSING");
    std::fprintf(f, "\n=== Config ===\n");
    std::fprintf(f, "preclear: %d\n", cfg::t.preclear);
    std::fprintf(f, "autobuy_guin: %d\n", cfg::t.autobuy_guin);
    std::fprintf(f, "autowin_bypass: %d\n", cfg::t.autowin_bypass);
    std::fprintf(f, "autowin: %d\n", cfg::t.autowin);
    std::fprintf(f, "autostack: %d\n", cfg::t.autostack);
    std::fprintf(f, "clear_stack: %d\n", cfg::t.clear_stack);
    std::fprintf(f, "\n=== Stack ===\n");
    std::fprintf(f, "count: %d/%d\n", cfg::stack_count, cfg::stack_target);

    std::fclose(f);
}

} // namespace status
