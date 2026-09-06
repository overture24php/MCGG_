#pragma once
// ---------------------------------------------------------------------------
// Toggle 6 fitur + status. Dibaca/ditulis dari file supaya bisa diubah tanpa
// rebuild: /sdcard/mcggmod.conf  (satu "key=0/1" per baris)
//
// Kenapa file, bukan ImGui dulu: overlay GL menambah 51 hook gl* dan risiko
// crash render. Buktikan 6 fitur jalan lebih dulu, UI menyusul.
// ---------------------------------------------------------------------------
#include <cstdint>

namespace cfg {

struct Toggles {
    // 1. beli slot murah di awal round, sisakan hero termahal
    bool preclear      = false;
    // 2. auto-beli slot yang MEMANG sudah ditandai gratis (Guinevere)
    bool autobuy_guin  = false;
    // 3. bersihkan flag invalid di paket hasil battle
    bool autowin_bypass = true;
    // 4. trigger auto win instan
    bool autowin        = false;
    // 5. beli semua hero 1g -> naik bintang 2, stop di 14 stack
    bool autostack      = false;
    // 6. reset counter stack (sekali pakai, otomatis balik false)
    bool clear_stack    = false;
};

extern Toggles t;

// counter Auto Stack (0..14)
extern int  stack_count;
extern int  stack_target;   // 14

void Load();          // baca /sdcard/mcggmod.conf
void StartWatcher();  // thread: reload tiap 2 detik + jalankan clear_stack

} // namespace cfg
