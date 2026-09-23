// ---------------------------------------------------------------------------
// Fitur 5: Auto Stack 14 + reset counter — port Mod.AutoStackTick +
// Patches.StackCounter. Mekanisme terbukti PC: beli SEMUA hero 1g di shop ->
// kalau habis refresh -> ulangi. Tiap hero 1g naik BINTANG 2 = 1 stack;
// berhenti tepat di 14.
//
// LIMA bug PC yang dihindari (Patches.cs 270-310):
//   1. hitung hanya newStarLevel == 2     2. hanya hero cost 1
//   3. hanya saat toggle autostack ON     4. berhenti tepat di target
//   5. dedup per hero GUID
// Filter cost 1 di Android: hero harus PERNAH terlihat berharga 1g di shop
// (game::IsCheapHero — dicatat otomatis saat baca harga slot; pengganti
// tabel ShopDiag.HeroCost versi PC yang tidak tersedia di Android).
//
// Signature dump v1.2.98.3143 (MCBattleData):
//   CraftAtReserve(accId x1, list x2, slot x3, guid x4, heroId x5, star x6)
//   CraftAtBattleField(accId x1, list x2, guid x3, heroId x4, skin x5, camp x6,
//     pos x7, rot stack[8..9], newHeroIndex 10, newStarLevel 11, type 12)
//   -> newStarLevel di CraftField = STACK arg index 11 (hook 16 arg).
// TIDAK pakai <set>/<vector> (NDK 29 rune table): array fixed + linear scan.
// ---------------------------------------------------------------------------
#include "features.h"
#include "game.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

#include <cstdint>

namespace feat {

static const int MAX_SEEN = 1000;
static uint32_t g_seen[MAX_SEEN];
static int      g_seenN = 0;
static int      g_fieldLog = 0;

// fase AutoStackTick (port Mod.cs 1761-1873)
static int64_t  g_stackTickLast = 0;
static int64_t  g_stackWaiting = 0;
static uint8_t  g_stackSlot = 255;
static int32_t  g_stackHero = 0;
static int      g_stackBuys = 0;
static int      g_stackRefreshes = 0;

static bool IsSeen(uint32_t guid) {
    for (int i = 0; i < g_seenN; i++)
        if (g_seen[i] == guid) return true;
    return false;
}

static bool AddSeen(uint32_t guid) {
    if (g_seenN >= MAX_SEEN) return false;
    g_seen[g_seenN++] = guid;
    return true;
}

void ResetStackState() {
    g_seenN = 0;
    cfg::stack_count = 0;
    g_stackSlot = 255;
    g_stackHero = 0;
    g_stackWaiting = 0;
    g_stackBuys = 0;
    g_stackRefreshes = 0;
}

void AutoStackOnMatchEnd() {
    LOGI("[STACK] match berakhir -> counter direset (beli %d, refresh %d)",
         g_stackBuys, g_stackRefreshes);
    ResetStackState();
    g_fieldLog = 0;
}

// port StackCounter.Count dengan 5 filter PC
static void CountCraft(uint64_t accId, uint32_t guid, int heroId, int star) {
    if (accId == 0 || guid == 0) return;
    uint64_t me = game::LocalAccIdStable();
    if (me != 0 && accId != me) return;                    // hanya milik sendiri
    if (star != 2) return;                                 // bug PC 1
    if (!game::IsCheapHero(heroId)) return;                // bug PC 2 (hero 1g)
    if (!cfg::t.autostack) return;                         // bug PC 3
    if (cfg::stack_count >= cfg::stack_target) return;     // bug PC 4
    if (IsSeen(guid)) return;                              // bug PC 5

    AddSeen(guid);
    cfg::stack_count++;
    LOGI("[STACK] %d/%d <- hero %d naik bintang 2",
         cfg::stack_count, cfg::stack_target, heroId);
    if (cfg::stack_count >= cfg::stack_target) {
        cfg::t.autostack = false;
        LOGI("[STACK] TARGET %d TERCAPAI -> Auto Stack dimatikan",
             cfg::stack_target);
    }
}

// IShowHandler_CraftHeroAtReserve — semua argumen register
static void OnCraftReserve(void** a) {
    uint32_t guid = (uint32_t)(uintptr_t)a[4];
    int heroId    = (int)(intptr_t)a[5];
    int star      = (int)(intptr_t)a[6];
    CountCraft((uint64_t)(uintptr_t)a[1], guid, heroId, star);
}

// IShowHandler_CraftHeroAtBattleField — newStarLevel di stack (index 11)
static void OnCraftField(void** a) {
    uint32_t guid = (uint32_t)(uintptr_t)a[3];
    int heroId    = (int)(intptr_t)a[4];
    int star      = (int)(intptr_t)a[11];
    if (g_fieldLog < 3) { // diagnosa layout argumen — verifikasi dari logcat
        g_fieldLog++;
        LOGI("[STACK] CraftField raw a3..a12: %u %d %d %d %d %d %d %d %d %d",
             (uint32_t)(uintptr_t)a[3], (int)(intptr_t)a[4], (int)(intptr_t)a[5],
             (int)(intptr_t)a[6], (int)(intptr_t)a[7], (int)(intptr_t)a[8],
             (int)(intptr_t)a[9], (int)(intptr_t)a[10], (int)(intptr_t)a[11],
             (int)(intptr_t)a[12]);
    }
    if (star < 1 || star > 8) return; // layout tidak sesuai -> jangan hitung
    CountCraft((uint64_t)(uintptr_t)a[1], guid, heroId, star);
}

// port Mod.AutoStackTick — dipanggil tiap frame dari game.cpp
void AutoStackFrame() {
    if (!cfg::t.autostack) return;
    if (cfg::stack_count >= cfg::stack_target) {
        cfg::t.autostack = false;
        LOGI("[STACK] target %d -> Auto Stack dimatikan", cfg::stack_target);
        return;
    }

    int64_t now = game::NowMs();

    // fase 2: konfirmasi slot masih berisi hero yang sama, lalu kirim op beli
    if (g_stackWaiting > 0) {
        if (now - g_stackWaiting < 80) return;
        g_stackWaiting = 0;
        void* shop = game::LocalShop();
        if (!shop) { g_stackSlot = 255; return; }
        int hero = -1;
        game::GetSlotPrice(shop, g_stackSlot, &hero);
        if (hero == g_stackHero && !game::ShopLocked(shop)) {
            if (game::SendBuy(g_stackSlot)) {
                g_stackBuys++;
                LOGI("[STACK] beli S%d hero=%d (1g) | stack %d/%d gold=%d",
                     g_stackSlot, g_stackHero, cfg::stack_count,
                     cfg::stack_target, game::Coin());
            }
        }
        g_stackSlot = 255;
        g_stackHero = 0;
        return;
    }

    if (now - g_stackTickLast < 80) return;

    void* shop = game::LocalShop();
    if (!shop) return;
    if (game::ShopLocked(shop)) return;

    int coin = game::Coin();

    // fase 1: cari hero 1g di shop.
    // Catatan: gate ReserveFreeSlots PC = safety net (selalu >=1 saat battle)
    // -> di Android dilewati; server yang menolak kalau bench penuh.
    int targetSlot = -1;
    for (int s = 0; s <= 4; s++) {
        int hero = -1;
        int price = game::GetSlotPrice(shop, s, &hero);
        if (price == 1 && hero > 0) {
            targetSlot = s;
            g_stackHero = hero;
            break;
        }
    }
    if (targetSlot >= 0) {
        g_stackTickLast = now;
        g_stackWaiting = now;
        g_stackSlot = (uint8_t)targetSlot;
        return;
    }

    // shop bersih dari hero 1g -> refresh (port Mod.cs 1849-1872)
    int rfCost = game::RefreshCost(shop);
    if (rfCost <= 0) rfCost = 2;
    if (coin < rfCost) {
        if (now - g_stackTickLast > 2000) {
            g_stackTickLast = now;
            LOGI("[STACK] menunggu: gold %d < biaya refresh %d (stack %d/%d)",
                 coin, rfCost, cfg::stack_count, cfg::stack_target);
        }
        return;
    }

    g_stackTickLast = now;
    // PC Mod.cs:1867 -> iCheatType = 0
    if (game::SendRefresh(0)) {
        g_stackRefreshes++;
        LOGI("[STACK] refresh #%d (tidak ada hero 1g, bayar %dg, sisa %dg)",
             g_stackRefreshes, rfCost, coin - rfCost);
    }
}

void InitAutoStack() {
    il2::Class* bd = il2::FindClass("", "MCBattleData");
    if (!bd) { LOGE("[STACK] MCBattleData TIDAK ADA"); return; }
    void* r = il2::MethodPtr(bd, "IShowHandler_CraftHeroAtReserve", -1);
    void* f = il2::MethodPtr(bd, "IShowHandler_CraftHeroAtBattleField", -1);
    hook::Attach(r, OnCraftReserve, nullptr, "CraftHeroAtReserve");
    hook::Attach(f, OnCraftField,   nullptr, "CraftHeroAtBattleField");
}

} // namespace feat
