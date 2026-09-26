// ---------------------------------------------------------------------------
// State bersama + hook bersama (dispatcher ke tiap fitur).
// Pemicu timeline PC dipusatkan di sini:
//   MCLogicHeroShop.Refresh / TryTriggerFreeBuyEvents -> RememberShop + arm
//   MCBattleData.IShowHandler_*  -> sync accId server + rescan free slot
//   UnityEngine.Time.get_deltaTime -> frame tick (pengganti OnUpdate PC)
// Semua di-resolve BY-NAME dari dump v1.2.98.3143 (dump_mcgg_android.cs).
// Dipanggil hanya dari thread game (sudah attach ke domain il2cpp).
// ---------------------------------------------------------------------------
#include "game.h"
#include "feature_api.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

#include <cstring>
#include <cstdio>
#include <ctime>

namespace game {

// ==== resolved saat Init() ==================================================
static il2::M m_getSlot;      // MCLogicHeroShop.GetSlotItem(int)
static il2::M m_getInfo;      // MCLogicHeroShop.GetItemInfo(int) -> struct (sret)
static il2::M m_locked;       // MCLogicHeroShop.GetShopLockStatus()
static il2::M m_getAccId;     // MCLogicBattleManager.get_m_uAccountId()
static il2::M m_getPd;        // MCLogicBattleManager.get_m_PlayerData()
static il2::M m_getLocal;     // MCLogicBattleManager.GetLocalPlayer()
static il2::M m_getInstance;  // LogicChessManager.get_Instance() (via Singleton<T>)
static il2::M m_getShop;      // LogicChessManager.GetLogicShop(accId)
static il2::M m_getCoin;      // MCChessPlayerData.get_Coin()
static il2::M m_curRound;     // Battle.MCLogicUtils.GetCurRound() (statis)
static il2::M m_sendOper;     // BattleReceiveMessageBridge.SendBattleOperData
static il2::Field* f_selfLbm = nullptr; // MCLogicBattleData.m_SelfLogicBattleManager
static il2::Field* f_selfAcc = nullptr; // MCLogicBattleData.m_SelfAccID
static il2::Class* kShop = nullptr;
static size_t off_lbm         = 0; // MCLogicHeroShop.lbm (0x18)
static size_t off_refreshCost = 0; // MCLogicHeroShop.refreshShopCost (0x5c)
static size_t off_chessPd     = 0; // LogicChessPlayer.m_ChessPlayerData (parent walk)
static size_t off_buyIndex    = 0; // OperType_Battle_BuyHeroFromShop.byIndex
static size_t off_cheatType   = 0; // OperType_Battle_RefreshShop.iCheatType

static const int32_t kOperBuy     = 89;
static const int32_t kOperRefresh = 91;

// ==== state =================================================================
static void*    g_shop = nullptr;
static uint64_t g_localAccId = 0;
static bool     g_wasInMatch = false;

// hero yang PERNAH terlihat berharga 1g di shop (filter Auto Stack; pengganti
// tabel ShopDiag.HeroCost versi PC)
static int g_cheap[256];
static int g_cheapN = 0;

int64_t NowMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint64_t ReadStaticU64(il2::Field* f) {
    uint64_t v = 0;
    if (f) il2::api.field_static_get_value(f, &v);
    return v;
}

void* SelfLbm() {
    void* p = nullptr;
    if (f_selfLbm) il2::api.field_static_get_value(f_selfLbm, &p);
    return p;
}

uint64_t SelfAccId() { return ReadStaticU64(f_selfAcc); }

uint64_t LocalAccIdStable() {
    if (g_localAccId) return g_localAccId;
    g_localAccId = SelfAccId();
    return g_localAccId;
}

void SetLocalAccId(uint64_t accId) {
    if (!accId) return;
    uint64_t me = SelfAccId();
    if (me && accId != me) return; // tolak accId pemain lain
    g_localAccId = accId;
}

bool InMatch() { return SelfLbm() != nullptr; }

void* ShopLbm(void* shop) {
    if (!shop || !off_lbm) return nullptr;
    return il2::FieldGet<void*>(shop, off_lbm, nullptr);
}

bool IsLocalShop(void* shop) {
    if (!shop) return false;
    void* lbm = ShopLbm(shop);
    if (!lbm) return false;
    void* self = SelfLbm();
    if (self) return lbm == self; // jalur utama: pointer lbm diri sendiri
    uint64_t me = LocalAccIdStable();
    if (!me || !m_getAccId.ok()) return false;
    uint64_t id = ((uint64_t (*)(void*, void*))m_getAccId.fn)(lbm, m_getAccId.mi);
    return id == me;
}

void RememberShop(void* shop) {
    if (IsLocalShop(shop)) g_shop = shop;
}

void* LocalShop() {
    void* s = g_shop;
    if (s && IsLocalShop(s)) return s;
    // fallback: LogicChessManager.GetLogicShop(accId) — hanya saat in-match
    uint64_t acc = LocalAccIdStable();
    if (acc && m_getInstance.ok() && m_getShop.ok()) {
        void* lcm = ((void* (*)(void*))m_getInstance.fn)(m_getInstance.mi);
        if (lcm) {
            void* shop = ((void* (*)(void*, uint64_t, void*))m_getShop.fn)(
                lcm, acc, m_getShop.mi);
            if (shop && IsLocalShop(shop)) { g_shop = shop; return shop; }
        }
    }
    return nullptr;
}

uint32_t CurRound() {
    if (!m_curRound.ok()) return 0;
    return ((uint32_t (*)(void*))m_curRound.fn)(m_curRound.mi);
}

int RefreshCost(void* shop) {
    if (!shop || !off_refreshCost) return -1;
    return il2::FieldGet<int32_t>(shop, off_refreshCost, -1);
}

void* LbmPlayerData(void* lbm) {
    if (!lbm || !m_getPd.ok()) return nullptr;
    return ((void* (*)(void*, void*))m_getPd.fn)(lbm, m_getPd.mi);
}

void* GetPlayerData(void* lbm) {
    if (!lbm || !m_getLocal.ok() || !off_chessPd) return nullptr;
    void* local = ((void* (*)(void*, void*))m_getLocal.fn)(lbm, m_getLocal.mi);
    if (!local) return nullptr;
    return il2::FieldGet<void*>(local, off_chessPd, nullptr);
}

int Coin() {
    void* pd = GetPlayerData(ShopLbm(LocalShop()));
    if (!pd || !m_getCoin.ok()) return -1;
    return ((int32_t (*)(void*, void*))m_getCoin.fn)(pd, m_getCoin.mi);
}

bool ShopLocked(void* shop) {
    if (!shop || !m_locked.ok()) return false;
    return ((bool (*)(void*, void*))m_locked.fn)(shop, m_locked.mi) != 0;
}

void* GetSlotItem(void* shop, int slot) {
    if (!shop || !m_getSlot.ok()) return nullptr;
    return ((void* (*)(void*, int32_t, void*))m_getSlot.fn)(shop, slot, m_getSlot.mi);
}

static void MarkCheap(int heroId) {
    if (heroId <= 0) return;
    for (int i = 0; i < g_cheapN; i++) if (g_cheap[i] == heroId) return;
    if (g_cheapN < 256) g_cheap[g_cheapN++] = heroId;
}

bool IsCheapHero(int heroId) {
    for (int i = 0; i < g_cheapN; i++) if (g_cheap[i] == heroId) return true;
    return false;
}

// offset struct MENTAH = offset dump - 0x10 (dump memakai header objek)
static size_t StructOff(size_t dumpOff) {
    return dumpOff >= 0x10 ? dumpOff - 0x10 : dumpOff;
}

int GetSlotPrice(void* shop, int slot, int* heroOut) {
    if (heroOut) *heroOut = 0;
    void* item = GetSlotItem(shop, slot);
    if (!item) return -1;
    static size_t off_hero = 0; // MCLogicHeroShopSlotItem.m_iHeroOrItemId (0x14)
    if (!off_hero)
        off_hero = il2::FieldOffset("", "MCLogicHeroShopSlotItem", "m_iHeroOrItemId");
    int hero = il2::FieldGet<int32_t>(item, off_hero, 0);
    if (hero <= 0) return -1;
    if (heroOut) *heroOut = hero;
    if (!m_getInfo.ok()) return -1;
    static size_t off_price_raw = 0; // m_iPrice 0x1c dump -> 0xc raw
    if (!off_price_raw)
        off_price_raw = StructOff(
            il2::FieldOffset("", "MCLogicHeroShopItemData", "m_iPrice"));
    // GetItemInfo -> STRUCT 24 byte = ABI sret (x0 = buffer):
    //   void GetItemInfo(ret*, this, slot, const MethodInfo*)
    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    auto fn = (void (*)(void*, void*, int32_t, void*))m_getInfo.fn;
    fn(buf, shop, slot, m_getInfo.mi);
    int price = il2::FieldGet<int32_t>(buf, off_price_raw, -1);
    if (price <= 0) return -1;
    if (price == 1) MarkCheap(hero); // catat utk filter Auto Stack
    return price;
}

bool SendBuy(uint8_t slot) {
    if (!m_sendOper.ok()) return false;
    void* op = il2::NewObject("MTTDProto", "OperType_Battle_BuyHeroFromShop");
    if (!op) { LOGE("[OP] gagal buat OperType_Battle_BuyHeroFromShop"); return false; }
    if (off_buyIndex) il2::FieldSet<uint8_t>(op, off_buyIndex, slot);
    auto fn = (void (*)(int32_t, void*, int32_t, int32_t, void*))m_sendOper.fn;
    fn(kOperBuy, op, 0, 0, m_sendOper.mi);
    return true;
}

bool SendRefresh(int cheatType) {
    if (!m_sendOper.ok()) return false;
    void* op = il2::NewObject("MTTDProto", "OperType_Battle_RefreshShop");
    if (!op) { LOGE("[OP] gagal buat OperType_Battle_RefreshShop"); return false; }
    if (off_cheatType) il2::FieldSet<int32_t>(op, off_cheatType, cheatType);
    auto fn = (void (*)(int32_t, void*, int32_t, int32_t, void*))m_sendOper.fn;
    fn(kOperRefresh, op, 0, 0, m_sendOper.mi);
    return true;
}

void ResetMatchState() {
    g_shop = nullptr;
    g_localAccId = 0;
    g_cheapN = 0;
    LOGI("[GAME] match berakhir -> state dibersihkan");
}

// ==== hook bersama =========================================================
// ---------------------------------------------------------------------------
// ⚠️ Argumen hook HARUS diambil di on_enter, BUKAN di on_leave.
// Di frida-gum, context saat on_leave berisi register SESUDAH fungsi kembali
// (x0 = return value); argumen asli sudah tidak ada. Membaca a[] di leave
// = baca sampah -> shop jadi null -> fitur diam tanpa log.
// Bool di arm64 AAPCS64 ada di byte terendah register, jadi dibaca per-byte.
// ---------------------------------------------------------------------------
static thread_local void*    tl_shop   = nullptr;   // MCLogicHeroShop (this)
static thread_local uint8_t  tl_isAuto = 0;         // Refresh.isAutoRefresh
static thread_local uint64_t tl_accId  = 0;         // IShowHandler_RefreshShop
static thread_local uint8_t  tl_slot   = 0;         // IShowHandler_BuyHero
static thread_local int32_t  tl_failId = 0;         // IShowHandler_BuyHeroFail
static thread_local void*    tl_pd     = nullptr;   // CheckFreeBuyHero (this)

static void OnRefreshEnter(void** a) {
    tl_shop   = a[0];
    tl_isAuto = *reinterpret_cast<uint8_t*>(&a[2]); // bool = low byte x2
    RememberShop(a[0]);
}

static void OnRefreshLeave(void**, void*) {
    void* shop   = tl_shop;
    bool  isAuto = tl_isAuto != 0;
    tl_shop = nullptr; tl_isAuto = 0;
    if (!shop) return;
    feat::PreClearOnRefreshLeave(shop, isAuto);
    feat::FreeBuyOnRefresh(shop);
}

static void OnTryTriggerEnter(void** a) {
    RememberShop(a[0]);
}

// IShowHandler_RefreshShop(accId x1, isAutoRefresh x2, ...)
static void OnSyncRefreshEnter(void** a) {
    tl_accId = reinterpret_cast<uintptr_t>(a[1]);
}
static void OnSyncRefreshLeave(void**, void*) {
    uint64_t accId = tl_accId;
    tl_accId = 0;
    if (!accId) return;
    SetLocalAccId(accId);
    feat::FreeBuyOnSyncRefresh(accId);
}

// IShowHandler_BuyHero(shopSlotIndex x1, m_ulAccountId x2, ...)
static void OnBuyHeroEnter(void** a) {
    tl_slot = *reinterpret_cast<uint8_t*>(&a[1]);
}
static void OnBuyHeroLeave(void**, void*) {
    uint8_t slot = tl_slot;
    tl_slot = 0;
    feat::FreeBuyOnBuyHero(slot);
}

// IShowHandler_BuyHeroFail(failTextId x1)
static void OnBuyFailEnter(void** a) {
    tl_failId = static_cast<int32_t>(reinterpret_cast<intptr_t>(a[1]));
}
static void OnBuyFailLeave(void**, void*) {
    int32_t id = tl_failId;
    tl_failId = 0;
    feat::FreeBuyOnBuyFail(id);
}

// CheckFreeBuyHero(this) -> bool
static void OnCheckFreeEnter(void** a) { tl_pd = a[0]; }
static void OnCheckFreeLeave(void**, void* ret) {
    void* pd = tl_pd;
    tl_pd = nullptr;
    if (!pd) return;
    if (reinterpret_cast<uintptr_t>(ret) != 0)
        feat::FreeBuyOnFreeChecked(pd);
}

// Frame tick: dipanggil tiap game membaca Time.get_deltaTime (banyak per frame).
// thread_local guard mencegah reentrancy; semua handler wajib murah.
static thread_local bool g_inFrame = false;
static void OnFrame(void**) {
    if (g_inFrame) return;
    g_inFrame = true;

    bool inMatch = InMatch();
    if (g_wasInMatch && !inMatch) {       // transisi match -> lobby
        ResetMatchState();
        feat::PreClearOnMatchEnd();
        feat::FreeBuyOnMatchEnd();
        feat::AutoStackOnMatchEnd();
    }
    g_wasInMatch = inMatch;

    // PENTING: frame handler hanya boleh jalan DI DALAM match. Di lobby/loading
    // objek shop/player masih null -> feat::*Frame() dereference null ->
    // SIGSEGV di UnityMain -> game restart terus (terbukti 26-09-2026).
    if (inMatch) {
        feat::PreClearFrame();
        feat::FreeBuyFrame();
        feat::AutoStackFrame();
    }

    g_inFrame = false;
}

void Init() {
    using namespace il2;

    kShop = FindClass("", "MCLogicHeroShop");
    if (kShop) {
        m_getSlot       = MethodFind(kShop, "GetSlotItem", 1);
        m_getInfo       = MethodFind(kShop, "GetItemInfo", 1);
        m_locked        = MethodFind(kShop, "GetShopLockStatus", 0);
        off_lbm         = FieldOffset(kShop, "lbm");
        off_refreshCost = FieldOffset(kShop, "refreshShopCost");
    } else {
        LOGE("[GAME] MCLogicHeroShop TIDAK ADA");
    }

    Class* kLbm = FindClass("", "MCLogicBattleManager");
    if (kLbm) {
        m_getAccId = MethodFind(kLbm, "get_m_uAccountId", 0);
        m_getPd    = MethodFind(kLbm, "get_m_PlayerData", 0);
        m_getLocal = MethodFind(kLbm, "GetLocalPlayer", 0);
    }

    Class* kLcm = FindClass("", "LogicChessManager");
    if (kLcm) {
        m_getInstance = MethodFind(kLcm, "get_Instance", 0); // dari Singleton<T>
        m_getShop     = MethodFind(kLcm, "GetLogicShop", 1);
    }

    Class* kPd = FindClass("", "MCChessPlayerData");
    if (kPd) m_getCoin = MethodFind(kPd, "get_Coin", 0);

    Class* kPlayer = FindClass("Battle", "LogicChessPlayer");
    if (kPlayer) off_chessPd = FieldOffset(kPlayer, "m_ChessPlayerData"); // parent walk

    Class* kUtils = FindClass("Battle", "MCLogicUtils");
    if (kUtils) m_curRound = MethodFind(kUtils, "GetCurRound", 0);

    Class* kBridge = FindClass("", "BattleReceiveMessageBridge");
    if (kBridge) m_sendOper = MethodFind(kBridge, "SendBattleOperData", 4);

    off_buyIndex  = FieldOffset("MTTDProto", "OperType_Battle_BuyHeroFromShop", "byIndex");
    off_cheatType = FieldOffset("MTTDProto", "OperType_Battle_RefreshShop", "iCheatType");

    Class* kbd = FindClass("", "MCLogicBattleData");
    if (kbd) {
        f_selfLbm = FieldFind(kbd, "m_SelfLogicBattleManager");
        f_selfAcc = FieldFind(kbd, "m_SelfAccID");
    }

    // ---- pasang hook pemicu ----
    // PENTING: hook sistem shop diintervensi saat game masih initializing, dan
    // pada beberapa versi itu memicu abort() di liblogic (game restart-loop).
    // Jadi hanya pasang kalau fiturnya benar-benar mau dipakai. AutoWin/Bypass
    // tidak bergantung hook shop, jadi tetap selalu aktif.
    const bool want_shop = cfg::t.preclear || cfg::t.autobuy_guin;
    LOGI("[GAME] hook shop %s (preclear=%d guin=%d)",
         want_shop ? "PASANG" : "lewati (fitur off)",
         (int)cfg::t.preclear, (int)cfg::t.autobuy_guin);

    if (kShop && want_shop) {
        hook::Attach(MethodPtr(kShop, "Refresh", 4), OnRefreshEnter, OnRefreshLeave,
                     "MCLogicHeroShop.Refresh");
        hook::Attach(MethodPtr(kShop, "TryTriggerFreeBuyEvents", 0), OnTryTriggerEnter,
                     nullptr, "MCLogicHeroShop.TryTriggerFreeBuyEvents");
    }

    Class* kbdh = FindClass("", "MCBattleData"); // IShowHandler_* ada di MCBattleData
    if (kbdh && want_shop) {
        hook::Attach(MethodPtr(kbdh, "IShowHandler_RefreshShop", 5), OnSyncRefreshEnter,
                     OnSyncRefreshLeave, "MCBattleData.IShowHandler_RefreshShop");
        hook::Attach(MethodPtr(kbdh, "IShowHandler_BuyHero", 3), OnBuyHeroEnter,
                     OnBuyHeroLeave, "MCBattleData.IShowHandler_BuyHero");
        hook::Attach(MethodPtr(kbdh, "IShowHandler_BuyHeroFail", 1), OnBuyFailEnter,
                     OnBuyFailLeave, "MCBattleData.IShowHandler_BuyHeroFail");
    }

    if (kPd && want_shop)
        hook::Attach(MethodPtr(kPd, "CheckFreeBuyHero", 1), OnCheckFreeEnter,
                     OnCheckFreeLeave, "MCChessPlayerData.CheckFreeBuyHero");

    // Frame tick: Time.get_deltaTime ada di UnityEngine.CoreModule.dll (bukan
    // Assembly-CSharp) -> cari image-nya dulu.
    Image* unity = FindImage("UnityEngine.CoreModule.dll");
    Class* kTime = unity ? FindClassIn(unity, "UnityEngine", "Time") : nullptr;
    if (kTime)
        hook::Attach(MethodPtr(kTime, "get_deltaTime", 0), OnFrame, nullptr,
                     "Time.get_deltaTime");
    else
        LOGE("[GAME] Time.get_deltaTime tidak ditemukan -> frame tick MATI "
             "(preclear/freebuy/autostack butuh tick)");

    LOGI("[GAME] shared init: lbm=0x%zx refreshCost=0x%zx chessPd=0x%zx send=%s time=%s",
         off_lbm, off_refreshCost, off_chessPd,
         m_sendOper.ok() ? "OK" : "MISS", kTime ? "OK" : "MISS");
}

} // namespace game

namespace feat {
void InitAll() {
    LOGI("[INIT] InitAutoWin ...");
    InitAutoWin();
    LOGI("[INIT] InitAutoStack ...");
    InitAutoStack();
    LOGI("[INIT] InitPreClear ...");
    InitPreClear();
    LOGI("[INIT] InitFreeBuy ...");
    InitFreeBuy();
    LOGI("[INIT] game::Init ...");
    game::Init(); // hook bersama terakhir (dispatcher -> handler fitur di atas)
    LOGI("[INIT] InitAll selesai");
}
} // namespace feat



