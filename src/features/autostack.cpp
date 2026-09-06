// ---------------------------------------------------------------------------
// Fitur 5: Auto Stack 14
// Fitur 6: Clear Stack (counter direset dari config watcher)
//
// Mekanisme (sesuai yang sudah terbukti di PC v0.14.x):
//   beli semua hero 1 gold di shop -> kalau habis, refresh shop -> ulangi.
//   1 hero 1g yang naik ke BINTANG 2 = 1 stack. Pada 14 stack, mati sendiri.
//
// Terverifikasi dari dump runtime v1.2.98.3143:
//   MCLogicHeroShop.GetSlotItem(Int32) -> MCLogicHeroShopSlotItem
//   MCLogicHeroShop.GetItemInfo(Int32) -> MCLogicHeroShopItemData (m_iPrice)
//   MCLogicHeroShop.GetShopLockStatus() -> Boolean
//   MCLogicHeroShop.refreshShopCost : Int32 (0x5c)
//   MTTDProto.OperType_Battle_RefreshShop { Int32 iCheatType }  id = 91
//   MTTDProto.OperType_Battle_BuyHeroFromShop { Byte byIndex }  id = 89
//   MCBattleData.IShowHandler_CraftHeroAtReserve(UInt64, List<UInt32>, ...)
//   MCBattleData.IShowHandler_CraftHeroAtBattleField(UInt64, List<UInt32>, ...)
//
// LIMA BUG versi PC yang WAJIB dihindari (semua sudah diterapkan di sini):
//   1. hitung hanya newStarLevel == 2  (bukan >= 2) -> bintang 3 jangan dihitung
//   2. hanya hero cost 1
//   3. hanya hitung saat toggle autostack ON
//   4. berhenti tepat di 14, jangan lewat
//   5. dedup per hero GUID
// Plus: deteksi match berakhir JANGAN pakai "shop != null" (di PC tidak pernah
// di-null -> counter menumpuk lintas match).
// ---------------------------------------------------------------------------
#include "features.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

#include <set>
#include <cstdint>

namespace feat {

// iCheatType: game sendiri mengirim 7747 saat refresh manual (temuan log PC).
// Mod PC mengirim 0 dan server tetap terima, tapi 7747 lebih menyerupai asli.
static const int32_t kCheatType = 7747;

static std::set<uint32_t> g_seenGuid;

static void CountCraft(uint64_t accId, int heroId, int newStar) {
    (void)accId;
    if (!cfg::t.autostack) return;                    // bug 3
    if (newStar != 2) return;                         // bug 1
    if (cfg::stack_count >= cfg::stack_target) return; // bug 4

    cfg::stack_count++;
    LOGI("[STACK] %d/%d  <- hero %d naik bintang 2",
         cfg::stack_count, cfg::stack_target, heroId);

    if (cfg::stack_count >= cfg::stack_target) {
        cfg::t.autostack = false;
        LOGI("[STACK] TARGET %d TERCAPAI -> Auto Stack dimatikan",
             cfg::stack_target);
    }
}

// IShowHandler_CraftHeroAtReserve(UInt64 accountId,
//     List<UInt32> m_CraftedHeroGuidList, Int32 slotIndex,
//     UInt32 newHeroGuid, Int32 newHeroId, Int32 newStarLevel)
// arm64: x0=this x1=accId x2=list x3=slot x4=guid x5=heroId x6=star
static void OnCraftReserve(void** a) {
    uint32_t guid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(a[4]));
    int heroId    = static_cast<int>(reinterpret_cast<intptr_t>(a[5]));
    int star      = static_cast<int>(reinterpret_cast<intptr_t>(a[6]));
    if (guid && !g_seenGuid.insert(guid).second) return;   // bug 5
    CountCraft(reinterpret_cast<uintptr_t>(a[1]), heroId, star);
}

// IShowHandler_CraftHeroAtBattleField(UInt64 accountId, ...,
//     UInt32 newHeroGuid, UInt32 newHeroId, UInt32 newStarLevel)
static void OnCraftField(void** a) {
    uint32_t guid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(a[3]));
    int heroId    = static_cast<int>(reinterpret_cast<uintptr_t>(a[4]));
    int star      = static_cast<int>(reinterpret_cast<uintptr_t>(a[5]));
    if (guid && !g_seenGuid.insert(guid).second) return;
    CountCraft(reinterpret_cast<uintptr_t>(a[1]), heroId, star);
}

void ResetStackState() {
    g_seenGuid.clear();
    cfg::stack_count = 0;
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
