// ---------------------------------------------------------------------------
// Fitur 2: Auto Buy slot GRATIS — port persis ScheduleAutoBuys/TryExecutePending
// (Mod.cs). Hanya membeli slot yang SUDAH ditandai gratis oleh game; TIDAK
// menyentuh m_FreeBuyHeroRate (menaikkannya = desync -> ketendang, README).
//
// Pemicu (sama PC): Refresh, IShowHandler_RefreshShop (sync server),
// CheckFreeBuyHero (flag baru dihitung). Eksekusi Pending dari frame tick
// (pengganti OnUpdate PC) — BuyDelayMs=0 + verifikasi ulang slot & flag.
// enum FreeBuyHeroType: None=0 ByTimes=1 ByRate=2 ByGoGoCard=3
// ByTimes throttle 2 detik (PC: LastByTimesBuyMs).
// ---------------------------------------------------------------------------
#include "feature_api.h"
#include "game.h"
#include "il2cpp.h"
#include "config.h"
#include "log.h"

#include <cstdint>

namespace feat {

static il2::M m_getFreeType; // MCChessPlayerData.GetFreeBuyHeroType(int)

struct PendingBuy {
    bool     active = false;
    uint8_t  slot   = 0;
    int32_t  heroId = 0;
    int32_t  type   = 0;   // FreeBuyHeroType
    int64_t  dueAt  = 0;
};
static const int kMaxPending = 16;
static PendingBuy g_pending[kMaxPending];
static int64_t g_lastByTimesMs = INT64_MIN / 2;

static void* PlayerData() {
    return game::GetPlayerData(game::ShopLbm(game::LocalShop()));
}

static int FreeTypeAt(void* pd, int slot) {
    if (!pd || !m_getFreeType.ok()) return 0;
    return ((int32_t (*)(void*, int32_t, void*))m_getFreeType.fn)(
        pd, slot, m_getFreeType.mi);
}

static PendingBuy* FindPending(int slot) {
    for (int i = 0; i < kMaxPending; i++)
        if (g_pending[i].active && g_pending[i].slot == slot) return &g_pending[i];
    return nullptr;
}

// port Mod.ScheduleAutoBuys — slot 0..9 (PC pakai 0..9 utk GetFreeBuyHeroType)
static void ScheduleAutoBuys(const char* reason) {
    if (!cfg::t.autobuy_guin) return;
    void* shop = game::LocalShop();
    void* pd = PlayerData();
    if (!shop || !pd) return;

    int64_t now = game::NowMs();
    bool byTimesSeen = false;
    for (int s = 0; s <= 9; s++) {
        int ft = FreeTypeAt(pd, s);
        if (ft == 0) continue; // None

        if (ft == 1) { // ByTimes: throttle 2s + maks 1 antrian (PC 1576-1582)
            if (now - g_lastByTimesMs < 2000) continue;
            if (byTimesSeen) continue;
            bool queued = false;
            for (int i = 0; i < kMaxPending; i++)
                if (g_pending[i].active && g_pending[i].type == 1) { queued = true; break; }
            if (queued) continue;
            byTimesSeen = true;
        }
        // ByRate(2)/ByGoGoCard(3): PC default BuyByRate/BuyByGoGoCard = true
        // -> tanpa filter tambahan.

        int hero = -1;
        game::GetSlotPrice(shop, s, &hero);
        void* item = game::GetSlotItem(shop, s);
        if (!item || hero <= 0) continue;

        PendingBuy* p = FindPending(s);
        if (p) { // perbarui isi bila slot berganti (PC 1587-1589)
            p->heroId = hero;
            p->type = ft;
            p->dueAt = now; // BuyDelayMs = 0
            continue;
        }
        for (int i = 0; i < kMaxPending; i++) {
            if (g_pending[i].active) continue;
            g_pending[i].active = true;
            g_pending[i].slot = (uint8_t)s;
            g_pending[i].heroId = hero;
            g_pending[i].type = ft;
            g_pending[i].dueAt = now;
            LOGI("[SCHEDULE] slot %d id=%d type=%d (%s)", s, hero, ft, reason);
            break;
        }
    }
}

// port Mod.TryExecutePending — jalan dari frame tick
static void TryExecutePending() {
    bool hasPending = false;
    for (int i = 0; i < kMaxPending; i++)
        if (g_pending[i].active) { hasPending = true; break; }
    if (!cfg::t.autobuy_guin || !hasPending) return;

    void* shop = game::LocalShop();
    void* lbm = game::ShopLbm(shop);
    void* pd = game::GetPlayerData(lbm);
    if (!shop || !lbm || !pd) return;

    int64_t now = game::NowMs();
    for (int i = 0; i < kMaxPending; i++) {
        PendingBuy& p = g_pending[i];
        if (!p.active) continue;
        if (now < p.dueAt) continue;
        if (game::ShopLocked(shop)) { p.dueAt = now + 50; continue; }

        // konteks manager harus data pemain lokal (PC 1626-1632: tunda 50ms)
        void* ctx = game::LbmPlayerData(lbm);
        if (ctx && pd && ctx != pd) { p.dueAt = now + 50; continue; }

        p.active = false;
        void* item = game::GetSlotItem(shop, p.slot);
        if (!item) { LOGI("[SKIP] slot %d kosong saat eksekusi", p.slot); continue; }
        int hero = -1;
        game::GetSlotPrice(shop, p.slot, &hero);
        if (hero != p.heroId) {
            LOGI("[SKIP] slot %d isi berubah (%d -> %d)", p.slot, p.heroId, hero);
            continue;
        }
        int ft = FreeTypeAt(pd, p.slot);
        if (ft == 0 || ft != p.type) {
            LOGI("[SKIP] slot %d flag free berubah (%d -> %d)", p.slot, p.type, ft);
            continue;
        }

        LOGI("[BUY] slot %d id=%d coin=%d", p.slot, hero, game::Coin());
        if (game::SendBuy((uint8_t)p.slot)) {
            if (p.type == 1) g_lastByTimesMs = now;
            LOGI("[AUTOBUY] => SendBattleOperData(BuyHeroFromShop, byIndex=%d) coin=%d",
                 p.slot, game::Coin());
        }
    }
}

// ---- dispatcher dari game.cpp ----
void FreeBuyOnRefresh(void*) { ScheduleAutoBuys("Refresh"); }

void FreeBuyOnSyncRefresh(uint64_t accId) {
    uint64_t me = game::LocalAccIdStable();
    if (me != 0 && accId != me) return; // SetLocalAccId sudah dicek di game.cpp
    LOGI("[SYNC] RefreshShop dikonfirmasi server (accId=%llu)",
         (unsigned long long)accId);
    ScheduleAutoBuys("Sync RefreshShop");
}

void FreeBuyOnFreeChecked(void*) { ScheduleAutoBuys("CheckFreeBuyHero"); }

void FreeBuyOnBuyHero(uint8_t slot) {
    LOGI("[SYNC] Pembelian dikonfirmasi server: slot=%d", slot);
}

void FreeBuyOnBuyFail(int32_t failId) {
    LOGW("[SYNC] Pembelian DITOLAK server (failTextId=%d)", failId);
}

void FreeBuyFrame() {
    if (!cfg::t.autobuy_guin) return;
    TryExecutePending();
}

void FreeBuyOnMatchEnd() {
    for (int i = 0; i < kMaxPending; i++) g_pending[i].active = false;
    g_lastByTimesMs = INT64_MIN / 2;
}

void InitFreeBuy() {
    il2::Class* pd = il2::FindClass("", "MCChessPlayerData");
    if (!pd) { LOGE("[GUIN] MCChessPlayerData TIDAK ADA"); return; }
    m_getFreeType = il2::MethodFind(pd, "GetFreeBuyHeroType", 1);
    LOGI("[GUIN] init: GetFreeBuyHeroType %s", m_getFreeType.ok() ? "OK" : "MISS");
}

} // namespace feat
