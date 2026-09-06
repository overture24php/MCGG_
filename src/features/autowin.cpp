// ---------------------------------------------------------------------------
// Fitur 3: Auto Win Bypass  (bersihkan flag invalid di paket hasil battle)
// Fitur 4: Auto Win         (trigger instan)
//
// Terverifikasi dari dump runtime v1.2.98.3143:
//   MTTDProto.Cmd_Battle_Result_CS.visit(MTTDProto.SdpPacker t, Boolean bOpt)
//     iBattleTime      UInt32  0x18
//     iWinCamp         UInt32  0x1c
//     iIsSurrender     Boolean 0x58
//     iIsInvalidBattle Boolean 0x70
//     iWarmBattleHook  Boolean 0x71
//     vHooker          List<UInt64> 0x78
//     sClientMD5       String  0x80
//   MCLogicBattleData.OnAutoWin(Boolean clearGuideID)   L276541
//
// PITFALL dari versi PC (v0.13.22) — JANGAN diulang:
//   Patch `visit` adalah SATU-SATUNYA titik yang dipakai. Memasang patch di
//   CountBattle.OnBeforeSendBattleResultCS menyebabkan stack overflow saat
//   scene battle dimuat.
//
// Offset field TIDAK di-hardcode: dibaca lewat il2cpp_field_get_offset supaya
// tetap benar walau game update.
// ---------------------------------------------------------------------------
#include "features.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

namespace feat {

static size_t off_battleTime = 0;
static size_t off_invalid    = 0;
static size_t off_hookFlag   = 0;
static size_t off_hookerList = 0;

// ambang invalid-battle di PC = 1500 (detik). Kalau iBattleTime di bawah ini
// server menandai match tidak sah.
static const uint32_t kMinBattleTime = 1500;

static int  g_patched = 0;

static void OnVisit(void** args) {
    if (!cfg::t.autowin_bypass) return;
    void* self = args[0];
    if (!self) return;

    uint32_t bt = il2::FieldGet<uint32_t>(self, off_battleTime, 0);
    if (off_battleTime && bt < kMinBattleTime)
        il2::FieldSet<uint32_t>(self, off_battleTime, kMinBattleTime);

    if (off_invalid)  il2::FieldSet<uint8_t>(self, off_invalid, 0);
    if (off_hookFlag) il2::FieldSet<uint8_t>(self, off_hookFlag, 0);

    // vHooker: List<UInt64>. Kosongkan dengan set _size = 0 (offset 0x18 di
    // List<T> il2cpp: [obj hdr 0x10][_items 0x10][_size 0x18]).
    if (off_hookerList) {
        void* lst = il2::FieldGet<void*>(self, off_hookerList, nullptr);
        if (lst) *reinterpret_cast<int32_t*>(
            reinterpret_cast<uint8_t*>(lst) + 0x18) = 0;
    }

    if (g_patched < 3) {
        g_patched++;
        LOGI("[BYPASS] visit: battleTime %u -> %u, invalid=0, hook=0, vHooker cleared",
             bt, bt < kMinBattleTime ? kMinBattleTime : bt);
    }
}

void InitAutoWin() {
    il2::Class* k = il2::FindClass("MTTDProto", "Cmd_Battle_Result_CS");
    if (!k) { LOGE("[BYPASS] Cmd_Battle_Result_CS TIDAK ADA"); return; }

    off_battleTime = il2::FieldOffset(k, "iBattleTime");
    off_invalid    = il2::FieldOffset(k, "iIsInvalidBattle");
    off_hookFlag   = il2::FieldOffset(k, "iWarmBattleHook");
    off_hookerList = il2::FieldOffset(k, "vHooker");
    LOGI("[BYPASS] offset: bt=0x%zx invalid=0x%zx hook=0x%zx vHooker=0x%zx",
         off_battleTime, off_invalid, off_hookFlag, off_hookerList);

    // visit ada 2 overload (SdpPacker & SdpUnpacker) -> argc=2 membedakan
    // keduanya masih ambigu, jadi ambil yang SdpPacker lewat pencarian argc.
    void* p = il2::MethodPtr(k, "visit", 2);
    hook::Attach(p, OnVisit, nullptr, "Cmd_Battle_Result_CS.visit");
}

// ---- Fitur 4: trigger auto win instan ----
// Dipanggil dari watcher saat toggle `autowin` dinyalakan.
void TriggerAutoWin() {
    il2::ScopedThread st;

    il2::Class* k = il2::FindClass("", "MCLogicBattleData");
    if (!k) { LOGE("[AUTOWIN] MCLogicBattleData TIDAK ADA"); return; }

    // Singleton<MCLogicBattleData>.Instance -> field statis m_Instance
    void* inst = nullptr;
    if (!il2::StaticGet("", "MCLogicBattleData", "m_Instance", &inst) || !inst) {
        // fallback: nama field statis versi lain
        il2::StaticGet("", "MCLogicBattleData", "s_Instance", &inst);
    }
    if (!inst) { LOGW("[AUTOWIN] instance null (belum dalam battle?)"); return; }

    void* fn = il2::MethodPtr(k, "OnAutoWin", 1);
    if (!fn) { LOGE("[AUTOWIN] OnAutoWin TIDAK ADA"); return; }

    reinterpret_cast<void (*)(void*, bool)>(fn)(inst, true);
    LOGI("[AUTOWIN] OnAutoWin(true) dipanggil");
}

} // namespace feat
