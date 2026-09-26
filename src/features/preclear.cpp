// ---------------------------------------------------------------------------
// Fitur 1: Shop Pre-Clear — port persis Scavenger.cs (PC).
// Di awal round (auto-refresh), beli semua slot harga DI BAWAH tertinggi,
// urut termurah; yang tersisa = hero termahal utk tarikan gratis Scavenger.
//
// Pemicu (sama PC Patch_ShopRefresh.Postfix + Scavenger.Tick):
//   - Refresh(isAuto=true) -> arming KALAU isi shop berubah (signature sig)
//   - frame tick           -> lanjutkan pekerjaan dalam window 200ms
// Pitfall PC yang dihindari (Scavenger.cs): OnStartPrepare BUKAN pemicu
// (jalan sebelum slot terisi, 8x/round); batas beli pakai `sent` saja
// (bukan _bought+sent); harga GetItemInfo().m_iPrice (ikut diskon/rule);
// verifikasi ulang harga tepat sebelum kirim op.
// ---------------------------------------------------------------------------
#include "feature_api.h"
#include "game.h"
#include "il2cpp.h"
#include "config.h"
#include "log.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace feat {

static const int64_t kWindowMs = 200; // Scavenger.cs WindowMs

// state per round (reset saat round berubah — port Scavenger)
static uint32_t g_lastRound = 0xFFFFFFFFu;
static int64_t  g_startedAt = 0;
static int      g_bought = 0;
static bool     g_done = false;
static bool     g_armed = false;
static char     g_sig[64] = "";
static int      g_keepPrice = 0;

struct SlotInfo { uint8_t slot; int heroId; int price; };

static void Finish(const char* why) {
    if (g_done) return;
    g_done = true;
    LOGI("[SCAV] R%u STOP -> %s", g_lastRound, why);
}

// signature isi shop: "id0,id1,...,id4"; hanya valid kalau >=2 slot terisi
static bool SigOf(void* shop, char* out, size_t outN) {
    out[0] = 0;
    int filled = 0;
    size_t pos = 0;
    for (int s = 0; s <= 4; s++) {
        int hero = 0;
        game::GetSlotPrice(shop, s, &hero); // sekalian catat harga 1g
        if (hero > 0) filled++;
        int n = snprintf(out + pos, outN - pos, "%s%d", s ? "," : "", hero);
        if (n < 0 || (size_t)n >= outN - pos) break;
        pos += (size_t)n;
    }
    return filled >= 2;
}

// inti — port Scavenger.Run()
static void Run(const char* reason) {
    if (!cfg::t.preclear) return;

    uint32_t round = game::CurRound();
    if (round != g_lastRound) {
        g_lastRound = round;
        g_done = false;
        g_armed = false;
        g_bought = 0;
        g_keepPrice = 0;
        g_startedAt = 0;
        g_sig[0] = 0;
    }
    if (g_done) return;
    if (!g_armed) return; // HANYA setelah auto-refresh awal round

    void* shop = game::LocalShop();
    if (!shop) return;

    if (g_startedAt != 0) {
        int64_t age = game::NowMs() - g_startedAt;
        if (age > kWindowMs) { Finish("window 200ms habis"); return; }
    }
    if (game::ShopLocked(shop)) { Finish("shop terkunci"); return; }

    SlotInfo slots[5];
    int n = 0;
    for (int s = 0; s < 5; s++) {
        int hero = 0;
        int price = game::GetSlotPrice(shop, s, &hero);
        if (price > 0) {
            slots[n].slot = (uint8_t)s;
            slots[n].heroId = hero;
            slots[n].price = price;
            n++;
        }
    }
    if (n < 2) return; // slot belum terisi ulang -> tunggu frame berikut

    if (g_keepPrice <= 0) {
        g_startedAt = game::NowMs();
        for (int i = 0; i < n; i++)
            if (slots[i].price > g_keepPrice) g_keepPrice = slots[i].price;
        LOGI("[SCAV] R%u mulai (%s) gold=%d | sisakan cost %d | %d slot terbaca",
             round, reason, game::Coin(), g_keepPrice, n);
    }

    SlotInfo targets[5];
    int tn = 0;
    for (int i = 0; i < n; i++)
        if (slots[i].price < g_keepPrice) targets[tn++] = slots[i];
    if (tn == 0) { Finish("semua slot harga sama"); return; }

    // termurah dulu (insertion sort, 5 elemen)
    for (int i = 1; i < tn; i++) {
        SlotInfo key = targets[i];
        int j = i - 1;
        while (j >= 0 && targets[j].price > key.price) {
            targets[j + 1] = targets[j];
            j--;
        }
        targets[j + 1] = key;
    }

    // batas beli = jumlah slot murah itu sendiri, pakai `sent` SAJA
    int maxBuy = tn;
    int coin = game::Coin();
    int spent = 0, sent = 0;
    bool goldShort = false;

    for (int i = 0; i < tn; i++) {
        if (sent >= maxBuy) break;
        int hero = 0;
        int nowPrice = game::GetSlotPrice(shop, targets[i].slot, &hero);
        if (nowPrice < 0) continue;            // slot sudah kosong
        if (nowPrice >= g_keepPrice) continue;  // sudah hero mahal
        if (coin - spent < nowPrice) { goldShort = true; continue; }
        if (game::SendBuy(targets[i].slot)) {
            spent += nowPrice;
            sent++;
            g_bought++;
            LOGI("[SCAV] beli S%d hero=%d (%dg) t=+%lldms", targets[i].slot,
                 hero, nowPrice, (long long)(game::NowMs() - g_startedAt));
        }
    }

    if (goldShort) { Finish("gold kurang"); return; }
    if (sent > 0) Finish("BERSIH, semua slot murah dibeli");
}

// ---- dispatcher dari game.cpp ----
void PreClearOnRefreshLeave(void* shop, bool isAuto) {
    static int dbg = 0;
    if (dbg < 5) {
        dbg++;
        LOGI("[SCAV] RefreshLeave shop=%p isAuto=%d preclear=%d", shop,
             (int)isAuto, (int)cfg::t.preclear);
    }
    if (!cfg::t.preclear) return;
    if (!isAuto) return;                 // hanya AUTO refresh awal round
    if (!game::IsLocalShop(shop)) { LOGW("[SCAV] bukan shop lokal, dilewati"); return; }
    if (g_done) return;
    char sig[64];
    if (!SigOf(shop, sig, sizeof(sig))) { LOGW("[SCAV] gagal baca isi shop"); return; }
    if (std::strcmp(sig, g_sig) == 0) return; // isi TIDAK berubah -> bukan refresh
    std::strncpy(g_sig, sig, sizeof(g_sig) - 1);
    g_armed = true;
    g_startedAt = 0;
    g_keepPrice = 0;
    LOGI("[SCAV] auto-refresh terdeteksi (isi baru: %s)", sig);
    Run("auto-refresh");
}

void PreClearFrame() {
    if (!cfg::t.preclear || !g_armed) return;
    Run("tick");
}

void PreClearOnMatchEnd() {
    g_armed = false;
    g_done = false;
    g_sig[0] = 0;
    g_keepPrice = 0;
    g_startedAt = 0;
}

void InitPreClear() {
    LOGI("[SCAV] init: port Scavenger (window %lldms)", (long long)kWindowMs);
}

} // namespace feat
