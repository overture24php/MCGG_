#pragma once
// ---------------------------------------------------------------------------
// Registrasi fitur + dispatcher hook bersama (semua hook TIMPAK/SHARED ada di
// game.cpp: Refresh, Time.get_deltaTime (frame tick), IShowHandler_*, dst).
// ---------------------------------------------------------------------------
#include <cstdint>

namespace feat {

void InitAutoWin();     // 3. bypass invalid + 4. trigger auto win + skip guide
void InitAutoStack();   // 5. auto stack 14 (dedup + filter hero 1g)
void InitPreClear();    // 1. shop pre-clear
void InitFreeBuy();     // 2. auto buy slot gratis
void InitAll();

void TriggerAutoWin();      // one-shot dari main loop (toggle autowin)
void TriggerSkipGuide();    // one-shot dari main loop (toggle skip_guide)
void ResetStackState();     // reset counter stack (clear_stack / awal match)

// ---- dispatcher dari game.cpp -> fitur ----
void PreClearOnRefreshLeave(void* shop, bool isAuto);
void PreClearFrame();       // hook frame: hanya set flag
void PreClearPump();       // thread mod: eksekusi pengiriman OP (aman)
void PreClearOnMatchEnd();
void FreeBuyOnRefresh(void* shop);
void FreeBuyOnSyncRefresh(uint64_t accId);
void FreeBuyOnFreeChecked(void* pd);
void FreeBuyOnBuyHero(uint8_t slot);
void FreeBuyOnBuyFail(int32_t failId);
void FreeBuyFrame();
void FreeBuyOnMatchEnd();
void AutoStackFrame();
void AutoStackOnMatchEnd();

} // namespace feat
