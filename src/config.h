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
    // 7. skip tutorial battle guide (sekali pakai, otomatis balik false)
    bool skip_guide     = false;
};

extern Toggles t;

// counter Auto Stack (0..14)
extern int  stack_count;
extern int  stack_target;   // 14

void Load();          // baca /sdcard/mcggmod.conf (one-shot = edge 0 -> 1)
void Save();          // tulis ulang file dari state `t` (reset one-shot ke 0)
void StartWatcher();  // thread: reload tiap 2 detik + jalankan clear_stack

// ---- bridge menu in-process (JNI, src/bridge.cpp) ----
// SetPersist: ubah persistent langsung di memori + tulis file; key di-LOCK
//   supaya Load() berikutnya tidak menimpa dari file lama (misal file tidak
//   bisa ditulis /sdcard karena scoped storage).
void SetPersist(const char* key, bool on);
bool GetPersist(const char* key);
// TriggerOnce: nyalakan one-shot di memori; loop main/watcher konsumsi dalam
//   <= 2 detik (tanpa lewat file — lebih cepat & tanpa tergantung izin storage).
void TriggerOnce(const char* key);

} // namespace cfg
