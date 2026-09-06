// ---------------------------------------------------------------------------
// Fitur 1: Shop Pre-Clear
//
// Tujuan: di awal round, beli slot-slot MURAH supaya yang tersisa di shop
// hanya hero TERMAHAL. Dengan begitu tarikan gratis bond Scavenger dipaksa
// mengambil hero mahal.
//
// Terverifikasi dari dump runtime v1.2.98.3143:
//   MCLogicHeroShop.Refresh(Battle.ShopRefreshCostType type,
//                           Boolean isAutoRefresh, Int32 refreshCost,
//                           Int32 playerLv)          <- PEMICU
//   MCLogicHeroShop.GetSlotItem(Int32 iSlot) -> MCLogicHeroShopSlotItem
//   MCLogicHeroShop.GetItemInfo(Int32 slot)  -> MCLogicHeroShopItemData
//   MCLogicHeroShop.GetShopLockStatus() -> Boolean
//   MTTDProto.OperType_Battle_BuyHeroFromShop { Byte byIndex }   id = 89
//   BattleReceiveMessageBridge.SendBattleOperData(OperTypeId_Battle,
//                   SdpWrapper, Int32 iMoveDirection, Boolean bNeedLock)
//
// PITFALL versi PC yang WAJIB dihindari (dari log nyata):
//   a) `OnStartPrepare` BUKAN pemicu yang benar. Terbukti jalan SEBELUM slot
//      diisi ulang (masih isi round sebelumnya) dan dipanggil 8x per round
//      (sekali untuk tiap pemain). Yang benar: Refresh dengan isAutoRefresh.
//   b) Batas beli HARUS pakai `sent` saja. Versi PC memakai `_bought + sent`
//      padahal keduanya naik di loop yang sama -> 1 beli terhitung 2, berhenti
//      di 2 slot, dan sisa slot murah justru ditarik Scavenger.
//   c) Harga dibaca dari GetItemInfo(slot).m_iPrice, BUKAN tabel cost hero
//      (biar diskon/rule khusus ikut terhitung).
//   d) Verifikasi ulang harga slot tepat sebelum kirim op (slot bisa terisi
//      ulang hero lain).
// ---------------------------------------------------------------------------
#include "features.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

// TIDAK pakai <vector> — NDK 29 butuh _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
// Pakai array fixed-size saja, cukup untuk 5 slot shop.
#include <cstdint>
#include <algorithm>

namespace feat {

static const int32_t kOperBuyHeroFromShop = 89;
static const int MAX_SLOTS = 5;

static size_t off_price     = 0;   // MCLogicHeroShopItemData.m_iPrice
static size_t off_slotHero  = 0;   // MCLogicHeroShopSlotItem.m_iHeroOrItemId

static void* fn_getSlotItem = nullptr;
static void* fn_getItemInfo = nullptr;
static void* fn_shopLocked  = nullptr;
static void* fn_sendOper    = nullptr;

struct SlotInfo { uint8_t slot; int heroId; int price; };

static int PriceOfSlot(void* shop, uint8_t s, int* heroIdOut) {
    *heroIdOut = 0;
    if (!fn_getSlotItem) return -1;

    void* item = reinterpret_cast<void* (*)(void*, int)>(fn_getSlotItem)(shop, s);
    if (!item) return -1;
    int hid = il2::FieldGet<int32_t>(item, off_slotHero, 0);
    if (hid <= 0) return -1;
    *heroIdOut = hid;

    if (!fn_getItemInfo || off_price == 0) return -1;
    void* info = reinterpret_cast<void* (*)(void*, int)>(fn_getItemInfo)(shop, s);
    if (!info) return -1;
    return il2::FieldGet<int32_t>(info, off_price, -1);
}

static bool SendBuy(uint8_t slot) {
    if (!fn_sendOper) return false;
    void* op = il2::NewObject("MTTDProto", "OperType_Battle_BuyHeroFromShop");
    if (!op) { LOGE("[SCAV] gagal buat OperType_Battle_BuyHeroFromShop"); return false; }

    size_t off = il2::FieldOffset("MTTDProto", "OperType_Battle_BuyHeroFromShop",
                                  "byIndex");
    if (off) il2::FieldSet<uint8_t>(op, off, slot);

    reinterpret_cast<void (*)(int32_t, void*, int32_t, bool)>(fn_sendOper)(
        kOperBuyHeroFromShop, op, 0, false);
    return true;
}

// Sort sederhana: insertion sort, 5 elemen saja
static void SortByPrice(SlotInfo* arr, int n) {
    for (int i = 1; i < n; i++) {
        SlotInfo key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j].price > key.price) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static void DoPreClear(void* shop) {
    if (!shop) return;

    if (fn_shopLocked &&
        reinterpret_cast<bool (*)(void*)>(fn_shopLocked)(shop)) {
        LOGI("[SCAV] shop terkunci, dilewati");
        return;
    }

    SlotInfo slots[MAX_SLOTS];
    int n = 0;
    for (uint8_t s = 0; s < MAX_SLOTS; s++) {
        int hid = 0;
        int price = PriceOfSlot(shop, s, &hid);
        if (price > 0) {
            slots[n].slot = s;
            slots[n].heroId = hid;
            slots[n].price = price;
            n++;
        }
    }
    if (n < 2) return;

    // cari harga tertinggi
    int keep = 0;
    for (int i = 0; i < n; i++) if (slots[i].price > keep) keep = slots[i].price;

    // kumpulkan yang murah
    SlotInfo targets[MAX_SLOTS];
    int tn = 0;
    for (int i = 0; i < n; i++) if (slots[i].price < keep) targets[tn++] = slots[i];

    if (tn == 0) {
        LOGI("[SCAV] semua slot harga sama (%d), tidak ada yang dibersihkan", keep);
        return;
    }

    // termurah dulu
    SortByPrice(targets, tn);

    LOGI("[SCAV] mulai: sisakan cost %d, target %d slot murah", keep, tn);

    int sent = 0;
    for (int i = 0; i < tn; i++) {
        if (sent >= tn) break;   // (b) batas pakai sent SAJA

        int hid = 0;
        int now = PriceOfSlot(shop, targets[i].slot, &hid);  // (d) verifikasi ulang
        if (now < 0) continue;
        if (now >= keep) continue;

        if (SendBuy(targets[i].slot)) {
            sent++;
            LOGI("[SCAV] beli S%d hero=%d (%dg)", targets[i].slot, hid, now);
        }
    }
    LOGI("[SCAV] selesai: %d/%d slot murah dibeli, sisa cost %d", sent, tn, keep);
}

// Refresh(ShopRefreshCostType type, Boolean isAutoRefresh, Int32 cost, Int32 lv)
// arm64: x0=this x1=type x2=isAutoRefresh x3=cost x4=lv
static void OnRefresh(void** a) {
    if (!cfg::t.preclear) return;
    bool isAuto = reinterpret_cast<uintptr_t>(a[2]) != 0;
    if (!isAuto) return;            // (a) hanya auto-refresh awal round
    DoPreClear(a[0]);
}

void InitPreClear() {
    il2::Class* shop = il2::FindClass("", "MCLogicHeroShop");
    if (!shop) { LOGE("[SCAV] MCLogicHeroShop TIDAK ADA"); return; }

    fn_getSlotItem = il2::MethodPtr(shop, "GetSlotItem", 1);
    fn_getItemInfo = il2::MethodPtr(shop, "GetItemInfo", 1);
    fn_shopLocked  = il2::MethodPtr(shop, "GetShopLockStatus", 0);

    off_slotHero = il2::FieldOffset("", "MCLogicHeroShopSlotItem", "m_iHeroOrItemId");
    off_price    = il2::FieldOffset("", "MCLogicHeroShopItemData", "m_iPrice");

    fn_sendOper = il2::MethodPtr("", "BattleReceiveMessageBridge",
                                 "SendBattleOperData", 4);

    LOGI("[SCAV] offset: slotHero=0x%zx price=0x%zx", off_slotHero, off_price);

    void* p = il2::MethodPtr(shop, "Refresh", 4);
    hook::Attach(p, OnRefresh, nullptr, "MCLogicHeroShop.Refresh");
}

} // namespace feat
