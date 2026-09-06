#pragma once
// Registrasi 6 fitur. Tiap fitur pasang hook-nya sendiri.
namespace feat {

void InitAutoWin();     // 3. bypass invalid  + 4. trigger auto win
void InitAutoStack();   // 5. auto stack 14   + 6. clear stack (counter)
void InitPreClear();    // 1. shop pre-clear
void InitFreeBuy();     // 2. auto buy Guinevere

void InitAll();

} // namespace feat
