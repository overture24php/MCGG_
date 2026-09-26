#pragma once
// ---------------------------------------------------------------------------
// State bersama + hook bersama antar fitur (pemicu timeline PC Mod.cs/Patches.cs/
// Scavenger.cs dipusatkan di sini):
//   - MCLogicHeroShop.Refresh           -> RememberShop + preclear arm + freebuy
//   - TryTriggerFreeBuyEvents           -> RememberShop
//   - MCBattleData.IShowHandler_*       -> sync accId server + rescan free slot
//   - UnityEngine.Time.get_deltaTime    -> frame tick (pengganti OnUpdate PC)
// Semua resolved BY-NAME dari dump v1.2.98.3143 (lihat dump_mcgg_android.cs).
// ---------------------------------------------------------------------------
#include <cstdint>

namespace game {

void Init();                    // resolve + pasang hook bersama

// ---- identitas / toko ----
void*  LocalShop();             // MCLogicHeroShop milik pemain lokal (cache+fallback)
bool   IsLocalShop(void* shop); // shop.lbm == m_SelfLogicBattleManager / accId cocok
void   RememberShop(void* shop); // dipanggil saat Refresh / TryTriggerFreeBuyEvents
uint64_t SelfAccId();           // MCLogicBattleData.m_SelfAccID (statis)
uint64_t LocalAccIdStable();    // cached accId lokal
void   SetLocalAccId(uint64_t accId); // hanya terima kalau cocok m_SelfAccID
void*  SelfLbm();               // MCLogicBattleData.m_SelfLogicBattleManager
bool   InMatch();
uint32_t CurRound();            // Battle.MCLogicUtils.GetCurRound()
int    RefreshCost(void* shop); // shop.refreshShopCost (fallback 2)
void*  LbmPlayerData(void* lbm);// lbm.get_m_PlayerData() (konteks vs pemain lokal)

// ---- akses data ----
void*  ShopLbm(void* shop);                 // shop.lbm (field 0x18)
void*  GetPlayerData(void* lbm);            // lbm.GetLocalPlayer().m_ChessPlayerData
int    Coin();                              // pd.get_Coin() / -1
bool   ShopLocked(void* shop);              // GetShopLockStatus()
void*  GetSlotItem(void* shop, int slot);   // GetSlotItem()
// Harga slot dari GetItemInfo(slot).m_iPrice (struct -> sret, offset RAW =
// offset dump - 0x10). heroOut = m_iHeroOrItemId. Return -1 kalau slot kosong.
// Slot harga 1g ikut dicatat ke daftar "hero murah" (filter Auto Stack).
int    GetSlotPrice(void* shop, int slot, int* heroOut);
bool   IsCheapHero(int heroId);             // pernah terlihat di harga 1g
void   ResetMatchState();                   // akhir match: cache & daftar dibersihkan

// ---- operasi server (op resmi, identik klik manual) ----
bool   SendBuy(uint8_t slot);               // OperTypeId 89 + byIndex
bool   SendRefresh(int cheatType);          // OperTypeId 91 (iCheatType, PC = 0)

int64_t NowMs();                            // monotonic ms (setara TickCount)

} // namespace game