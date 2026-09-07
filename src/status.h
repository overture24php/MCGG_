#pragma once
// ---------------------------------------------------------------------------
// Status file: /sdcard/mcggmod_status.txt
// Diupdate tiap detik. Buka di file manager untuk lihat status real-time
// tanpa logcat. Berguna untuk diagnose.
// ---------------------------------------------------------------------------
namespace status {

void Update();   // panggil tiap detik dari main loop

} // namespace status
