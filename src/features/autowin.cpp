// ---------------------------------------------------------------------------
// Fitur 3: Auto Win Bypass — patch visit(SdpPacker) di TIGA paket hasil.
// Fitur 4: Trigger Auto Win (OnAutoWin) + Skip Tutorial Battle Guide.
//
// Port PC Mod.cs TryPatchVisit + ApplyAutoWinBypass + LogRound_visit_Prefix +
// SuiviveOnePlayer + TriggerAutoWin + TriggerSkipGuide.
// PITFALL PC: CUKUP patch visit() (titik serialisasi terakhir). Patch
// CountBattle.OnBeforeSendBattleResultCS = stack overflow 0xc00000fd saat
// scene battle dimuat.
//
// Dump v1.2.98.3143:
//   Cmd_Battle_Result_CS: iBattleTime 0x18, iIsInvalidBattle 0x70,
//     iWarmBattleHook 0x71, bIsDownLoadComplated 0x72, vHooker 0x78
//   Cmd_Battle_LogRound_CS: uiRound 0x10, uiRoundBattleTime 0x14,
//     uiRoundSelfBattleTime 0x18
//   Cmd_Suivive_OnePlayer_Battle_Result_CS: iBattleTime 0x18
//   MCReportBattleData: _fightDuringTime 0x18, battleDataMsg 0x88 (LogRound)
//   Singleton<T>._instance (statis, parent walk)
// MinReportBattleTime = 1200 — SAMA dengan PC Mod.cs (bukan 1500).
// ---------------------------------------------------------------------------
#include "feature_api.h"
#include "game.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

namespace feat {

static size_t off_bt = 0, off_invalid = 0, off_hook = 0;
static size_t off_downloaded = 0, off_hooker = 0;
static size_t off_lrRound = 0, off_lrTime = 0, off_lrSelf = 0;
static size_t off_svBt = 0;
static int g_log = 0;

static const uint32_t kMinBattleTime = 1200; // PC Mod.MinReportBattleTime

static void OnResultVisit(void** a) {
    if (!cfg::t.autowin_bypass) return;
    void* self = a[0];
    if (!self) return;

    uint32_t bt = il2::FieldGet<uint32_t>(self, off_bt, 0);
    if (off_bt && bt < kMinBattleTime)
        il2::FieldSet<uint32_t>(self, off_bt, kMinBattleTime);
    if (off_invalid)    il2::FieldSet<uint8_t>(self, off_invalid, 0);
    if (off_hook)       il2::FieldSet<uint8_t>(self, off_hook, 0);
    if (off_downloaded) il2::FieldSet<uint8_t>(self, off_downloaded, 1);

    // vHooker List<UInt64>: kosongkan lewat _size=0 (layout List il2cpp:
    // [obj 0x10][_items 0x10][_size 0x18]) — PC pakai Clear().
    if (off_hooker) {
        void* lst = il2::FieldGet<void*>(self, off_hooker, nullptr);
        if (lst)
            *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(lst) + 0x18) = 0;
    }

    if (g_log < 3) {
        g_log++;
        LOGI("[BYPASS] visit: bt %u -> %u, invalid=0, hook=0, downloaded=1, vHooker cleared",
             bt, bt < kMinBattleTime ? kMinBattleTime : bt);
    }
}

static void OnLogRoundVisit(void** a) {
    if (!cfg::t.autowin_bypass) return;
    void* self = a[0];
    if (!self) return;
    bool changed = false;
    if (off_lrRound && il2::FieldGet<uint32_t>(self, off_lrRound, 0) < 4u) {
        il2::FieldSet<uint32_t>(self, off_lrRound, 4u); changed = true;
    }
    if (off_lrTime && il2::FieldGet<uint32_t>(self, off_lrTime, 0) < 300u) {
        il2::FieldSet<uint32_t>(self, off_lrTime, 300u); changed = true;
    }
    if (off_lrSelf && il2::FieldGet<uint32_t>(self, off_lrSelf, 0) < 300u) {
        il2::FieldSet<uint32_t>(self, off_lrSelf, 300u); changed = true;
    }
    if (changed && g_log < 6) {
        g_log++;
        LOGI("[EXP0] loground: round>=4, roundTime>=300, selfTime>=300");
    }
}

static void OnSuiviveVisit(void** a) {
    if (!cfg::t.autowin_bypass) return;
    void* self = a[0];
    if (!self || !off_svBt) return;
    uint32_t bt = il2::FieldGet<uint32_t>(self, off_svBt, 0);
    if (bt < kMinBattleTime) {
        il2::FieldSet<uint32_t>(self, off_svBt, kMinBattleTime);
        LOGI("[AUTOWIN] Suivive bypass: time=%u", kMinBattleTime);
    }
}

// Singleton<T> -> _instance (statis) / get_Instance(), jalan untuk
// MCLogicBattleData / MCReportBattleData / GuideManager (parent walk).
static void* SingletonInstance(il2::Class* k) {
    if (!k) return nullptr;
    il2::Field* f = il2::FieldFind(k, "_instance");
    if (f) {
        void* inst = nullptr;
        il2::api.field_static_get_value(f, &inst);
        if (inst) return inst;
    }
    il2::M gi = il2::MethodFind(k, "get_Instance", 0);
    if (gi.ok()) return ((void* (*)(void*))gi.fn)(gi.mi);
    return nullptr;
}

void TriggerAutoWin() {
    il2::ScopedThread st;

    // Di luar match singleton belum dibuat -> memanggilnya = SIGABRT.
    // (Fungsi ini biasanya dipakai saat match; guard tetap dipasang biar aman.)
    {
        il2::Class* kChk = il2::FindClass("", "MCLogicBattleData");
        void* chk = nullptr;
        il2::Field* fChk = kChk ? il2::FieldFind(kChk, "_instance") : nullptr;
        if (fChk) il2::api.field_static_get_value(fChk, &chk);
        if (!chk) { LOGW("[AUTOWIN] di luar match, dilewati"); return; }
    }

    // 1. MCReportBattleData: fightDuringTime + battleDataMsg (PC 1903-1921)
    il2::Class* kRd = il2::FindClass("", "MCReportBattleData");
    void* rd = SingletonInstance(kRd);
    if (rd && kRd) {
        size_t offFdt = il2::FieldOffset(kRd, "_fightDuringTime");
        if (offFdt) il2::FieldSet<uint32_t>(rd, offFdt, kMinBattleTime);
        size_t offMsg = il2::FieldOffset(kRd, "battleDataMsg");
        void* bm = offMsg ? il2::FieldGet<void*>(rd, offMsg, nullptr) : nullptr;
        if (bm) {
            il2::Class* kLr = il2::FindClass("MTTDProto", "Cmd_Battle_LogRound_CS");
            if (kLr) {
                size_t o1 = il2::FieldOffset(kLr, "uiRound");
                size_t o2 = il2::FieldOffset(kLr, "uiRoundBattleTime");
                size_t o3 = il2::FieldOffset(kLr, "uiRoundSelfBattleTime");
                if (o1 && il2::FieldGet<uint32_t>(bm, o1, 0) < 4u)
                    il2::FieldSet<uint32_t>(bm, o1, 4u);
                if (o2 && il2::FieldGet<uint32_t>(bm, o2, 0) < 300u)
                    il2::FieldSet<uint32_t>(bm, o2, 300u);
                if (o3 && il2::FieldGet<uint32_t>(bm, o3, 0) < 300u)
                    il2::FieldSet<uint32_t>(bm, o3, 300u);
            }
        }
    } else {
        LOGW("[AUTOWIN] MCReportBattleData instance NULL");
    }

    // 2. MCLogicBattleData.OnAutoWin(true)
    il2::Class* kBd = il2::FindClass("", "MCLogicBattleData");
    void* inst = SingletonInstance(kBd);
    if (!inst) { LOGW("[AUTOWIN] MCLogicBattleData.Instance NULL (belum di battle?)"); return; }
    il2::M onAuto = il2::MethodFind(kBd, "OnAutoWin", 1);
    if (!onAuto.ok()) { LOGE("[AUTOWIN] OnAutoWin TIDAK ADA"); return; }
    ((void (*)(void*, bool, void*))onAuto.fn)(inst, true, onAuto.mi);
    LOGI("[AUTOWIN] OnAutoWin(true) dipanggil (fightDuringTime=%u)", kMinBattleTime);
}

// Hanya panggil method game kalau BENAR-BENAR dalam match. Di layar login
// singleton GuideManager / MCReportBattleData belum dibuat -> memanggilnya
// = SIGABRT di liblogic (terbukti). Game hanya membuat instance
// MCLogicBattleData saat match dimulai, jadi itu penanda paling murah.
static bool InMatch() {
    il2::Class* k = il2::FindClass("", "MCLogicBattleData");
    if (!k) return false;
    void* inst = nullptr;
    il2::Field* f = il2::FieldFind(k, "_instance");
    if (f) il2::api.field_static_get_value(f, &inst);
    if (inst) return true;
    // fallback: cek battle manager instance
    il2::Field* m = il2::FieldFind(k, "m_SelfLogicBattleManager");
    if (!m) return false;
    void* lbm = nullptr;
    il2::api.field_static_get_value(m, &lbm);
    return lbm != nullptr;
}

void TriggerSkipGuide() {
    il2::ScopedThread st;
    if (!InMatch()) { LOGW("[SKIP] di luar match, dilewati"); return; }
    il2::Class* k = il2::FindClass("", "GuideManager");
    if (!k) { LOGW("[SKIP] class GuideManager null"); return; }
    void* inst = SingletonInstance(k);
    if (!inst) { LOGW("[SKIP] GuideManager.Instance NULL"); return; }
    il2::M fn = il2::MethodFind(k, "SkipTutorialBattleGuide", 1);
    LOGI("[SKIP] inst=%p fn.fn=%p fn.mi=%p", inst, fn.fn, fn.mi);
    if (!fn.ok() || !fn.mi) { LOGE("[SKIP] SkipTutorialBattleGuide TIDAK ADA/pointer null"); return; }
    // Jaga tambahan: pointer method harus berada di region executable modul —
    // kalau tidak, panggilan = jump ke 0x0 (SIGABRT).
    if (((uintptr_t)fn.fn & 0x3) != 0 || !fn.fn) {
        LOGE("[SKIP] fn pointer rusak, dilewati");
        return;
    }
    ((void (*)(void*, bool, void*))fn.fn)(inst, true, fn.mi);
    LOGI("[SKIP] SkipTutorialBattleGuide(true) dipanggil");
}

void InitAutoWin() {
    // Hook visit() hanya dipasang kalau bypassDiminta. intervened prolog fungsi
    // saat game masih initializing bisa memicu restart loop; jadi default-nya
    // TIDAK ada hook sama sekali kalau fitur off.
    if (!cfg::t.autowin_bypass) {
        LOGI("[BYPASS]OFF, hook visit tidak dipasang");
        return;
    }
    // --- Cmd_Battle_Result_CS.visit(SdpPacker, bool) ---
    LOGI("[BYPASS] cari Cmd_Battle_Result_CS ...");
    il2::Class* kRes = il2::FindClass("MTTDProto", "Cmd_Battle_Result_CS");
    LOGI("[BYPASS] ketemu=%d", (int)(kRes != nullptr));
    if (kRes) {
        LOGI("[BYPASS] field iBattleTime ...");
        off_bt         = il2::FieldOffset(kRes, "iBattleTime");
        LOGI("[BYPASS] field iIsInvalidBattle ...");
        off_invalid    = il2::FieldOffset(kRes, "iIsInvalidBattle");
        LOGI("[BYPASS] field iWarmBattleHook ...");
        off_hook       = il2::FieldOffset(kRes, "iWarmBattleHook");
        LOGI("[BYPASS] field bIsDownLoadComplated ...");
        off_downloaded = il2::FieldOffset(kRes, "bIsDownLoadComplated");
        LOGI("[BYPASS] field vHooker ...");
        off_hooker     = il2::FieldOffset(kRes, "vHooker");
        // 2 overload visit(argc=2) -> disambiguate via nama class arg ke-0
        LOGI("[BYPASS] MethodFindArg0 visit(SdpPacker) ...");
        il2::M v = il2::MethodFindArg0(kRes, "visit", 2, "SdpPacker");
        LOGI("[BYPASS] MethodFindArg0 ok=%d fn=%p", (int)v.ok(), v.fn);
        if (v.ok())
            hook::Attach(v.fn, OnResultVisit, nullptr, "Cmd_Battle_Result_CS.visit(SdpPacker)");
    } else {
        LOGE("[BYPASS] Cmd_Battle_Result_CS TIDAK ADA");
    }
    LOGI("[BYPASS] offset: bt=0x%zx invalid=0x%zx hook=0x%zx dl=0x%zx vHooker=0x%zx",
         off_bt, off_invalid, off_hook, off_downloaded, off_hooker);

    // --- Cmd_Battle_LogRound_CS.visit(SdpPacker, bool) ---
    il2::Class* kLr = il2::FindClass("MTTDProto", "Cmd_Battle_LogRound_CS");
    if (kLr) {
        off_lrRound = il2::FieldOffset(kLr, "uiRound");
        off_lrTime  = il2::FieldOffset(kLr, "uiRoundBattleTime");
        off_lrSelf  = il2::FieldOffset(kLr, "uiRoundSelfBattleTime");
        il2::M v = il2::MethodFindArg0(kLr, "visit", 2, "SdpPacker");
        if (v.ok())
            hook::Attach(v.fn, OnLogRoundVisit, nullptr, "Cmd_Battle_LogRound_CS.visit(SdpPacker)");
    } else {
        LOGE("[EXP0] Cmd_Battle_LogRound_CS TIDAK ADA");
    }

    // --- Cmd_Suivive_OnePlayer_Battle_Result_CS.visit(SdpPacker, bool) ---
    il2::Class* kSv = il2::FindClass("MTTDProto", "Cmd_Suivive_OnePlayer_Battle_Result_CS");
    if (kSv) {
        off_svBt = il2::FieldOffset(kSv, "iBattleTime");
        il2::M v = il2::MethodFindArg0(kSv, "visit", 2, "SdpPacker");
        if (v.ok())
            hook::Attach(v.fn, OnSuiviveVisit, nullptr,
                         "Cmd_Suivive_OnePlayer_Battle_Result_CS.visit(SdpPacker)");
    } else {
        LOGW("[AUTOWIN] Cmd_Suivive_OnePlayer_Battle_Result_CS tidak ada");
    }
}

} // namespace feat
