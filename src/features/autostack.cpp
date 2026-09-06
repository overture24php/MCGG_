// ---------------------------------------------------------------------------
// Fitur 5: Auto Stack 14
// Fitur 6: Clear Stack (counter direset dari config watcher)
//
// Mekanisme (sesuai yang sudah terbukti di PC v0.14.x):
//   beli semua hero 1 gold di shop -> kalau habis, refresh shop -> ulangi.
//   1 hero 1g yang naik ke BINTANG 2 = 1 stack. Pada 14 stack, mati sendiri.
//
// Terverifikasi dari dump runtime v1.2.98.3143:
//   MCBattleData.IShowHandler_CraftHeroAtReserve(UInt64, List<UInt32>, ...)
//   MCBattleData.IShowHandler_CraftHeroAtBattleField(UInt64, List<UInt32>, ...)
//
// LIMA BUG versi PC yang WAJIB dihindari:
//   1. hitung hanya newStarLevel == 2
//   2. hanya hero cost 1
//   3. hanya hitung saat toggle autostack ON
//   4. berhenti tepat di 14
//   5. dedup per hero GUID
// Plus: deteksi match berakhir JANGAN pakai "shop != null".
//
// TIDAK pakai <set> — NDK 29 butuh _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE.
// Pakai array fixed + linear scan: max 1000 GUID unik per match, cukup.
// ---------------------------------------------------------------------------
#include "features.h"
#include "il2cpp.h"
#include "hook.h"
#include "config.h"
#include "log.h"

#include <cstdint>

namespace feat {

static const int32_t kCheatType = 7747;
static const int   MAX_SEEN = 1000;

static uint32_t g_seen[MAX_SEEN];
static int      g_seenCount = 0;

static void ClearSeen() {
    g_seenCount = 0;
}

static bool IsSeen(uint32_t guid) {
    for (int i = 0; i < g_seenCount; i++)
        if (g_seen[i] == guid) return true;
    return false;
}

static bool AddSeen(uint32_t guid) {
    if (g_seenCount >= MAX_SEEN) return false;
    g_seen[g_seenCount++] = guid;
    return true;
}

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
    if (guid && !IsSeen(guid)) {
        AddSeen(guid);                                // bug 5
        CountCraft(reinterpret_cast<uintptr_t>(a[1]), heroId, star);
    }
}

// IShowHandler_CraftHeroAtBattleField(UInt64 accountId, ...,
//     UInt32 newHeroGuid, UInt32 newHeroId, UInt32 newStarLevel)
static void OnCraftField(void** a) {
    uint32_t guid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(a[3]));
    int heroId    = static_cast<int>(reinterpret_cast<uintptr_t>(a[4]));
    int star      = static_cast<int>(reinterpret_cast<intptr_t>(a[5]));
    if (guid && !IsSeen(guid)) {
        AddSeen(guid);
        CountCraft(reinterpret_cast<uintptr_t>(a[1]), heroId, star);
    }
}

void ResetStackState() {
    ClearSeen();
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
