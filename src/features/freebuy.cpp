// ---------------------------------------------------------------------------
// Fitur 2: Auto Buy Guinevere (free-buy)
//
// Yang dilakukan: auto-beli slot yang MEMANG sudah ditandai gratis oleh game.
// TIDAK menaikkan jumlah/peluang free-buy.
//
// Terverifikasi dari dump runtime v1.2.98.3143:
//   MCChessPlayerData.GetFreeBuyHeroType(Int32 slot) -> FreeBuyHeroType
//   MCChessPlayerData.CheckFreeBuyHero(Dictionary<Int32,Int32>) -> Boolean
//   MCChessPlayerData.OnUseFreeBuy(FreeBuyHeroType type)
//   MCChessPlayerData.m_FreeBuyHeroRate : Astar.int3 (0x7c)   nilai asli 2,1,1
//
// PERINGATAN PENTING (temuan PC, jangan diulang di Android):
//   m_FreeBuyHeroRate komponen y/z = JUMLAH hero gratis per pemicu, bukan
//   peluang. Menaikkannya di PvP = DESYNC: peer memakai config asli 2,1,1
//   sehingga di sisi mereka hero ke-2..ke-5 DIPOTONG GOLD. Selisih itu lahir
//   diam-diam saat beli dan meledak saat hero DIJUAL (gold ditambah dengan
//   basis berbeda) -> ketendang.
//   Karena itu fitur ini TIDAK menyentuh m_FreeBuyHeroRate sama sekali.
//
// enum FreeBuyHeroType: None=0, ByTimes=1, ByRate=2, ByGoGoCard=3
// ---------------------------------------------------------------------------
#include "features.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

#include <cstdint>

namespace feat {

static const int32_t kOperBuyHeroFromShop = 89;

static void* fn_getFreeType = nullptr;
static void* fn_sendOper    = nullptr;
static int   g_bought       = 0;

static bool SendBuy(uint8_t slot) {
    if (!fn_sendOper) return false;
    void* op = il2::NewObject("MTTDProto", "OperType_Battle_BuyHeroFromShop");
    if (!op) return false;
    size_t off = il2::FieldOffset("MTTDProto", "OperType_Battle_BuyHeroFromShop",
                                 "byIndex");
    if (off) il2::FieldSet<uint8_t>(op, off, slot);
    reinterpret_cast<void (*)(int32_t, void*, int32_t, bool)>(fn_sendOper)(
        kOperBuyHeroFromShop, op, 0, false);
    return true;
}

// CheckFreeBuyHero(Dictionary<Int32,Int32> slotHeroes) -> Boolean
// Dipanggil game saat menentukan slot mana yang gratis. Kita pakai sebagai
// pemicu: kalau hasilnya true, cek tiap slot lalu beli yang bertanda gratis.
// arm64: x0=this(MCChessPlayerData) x1=dict
static void OnCheckFreeBuy(void** a, void* ret) {
    if (!cfg::t.autobuy_guin) return;
    bool any = reinterpret_cast<uintptr_t>(ret) != 0;
    if (!any) return;

    void* pd = a[0];
    if (!pd || !fn_getFreeType) return;

    for (int s = 0; s <= 4; s++) {
        int type = reinterpret_cast<int (*)(void*, int)>(fn_getFreeType)(pd, s);
        if (type == 0) continue;   // None
        if (SendBuy(static_cast<uint8_t>(s))) {
            g_bought++;
            LOGI("[GUIN] beli gratis S%d tipe=%d (total %d)", s, type, g_bought);
        }
    }
}

void InitFreeBuy() {
    il2::Class* pd = il2::FindClass("", "MCChessPlayerData");
    if (!pd) { LOGE("[GUIN] MCChessPlayerData TIDAK ADA"); return; }

    fn_getFreeType = il2::MethodPtr(pd, "GetFreeBuyHeroType", 1);
    fn_sendOper    = il2::MethodPtr("", "BattleReceiveMessageBridge",
                                    "SendBattleOperData", 4);

    void* p = il2::MethodPtr(pd, "CheckFreeBuyHero", 1);
    hook::Attach(p, nullptr, OnCheckFreeBuy, "CheckFreeBuyHero");
}

} // namespace feat
