#pragma once
// ---------------------------------------------------------------------------
// Menu overlay pakai Dear ImGui (pola libTool).
//
// Cara kerja:
//   - Hook eglSwapBuffers (libEGL.so) -> panggil Render() sebelum swap
//   - Render: ImGui frame -> overlay半透明 dengan checkbox 6 fitur
//   - Input: ImGui handle touch sendiri (tidak perlu hook input game)
//
// Kenapa ImGui dan bukan logcat: biar user BISA tau fitur jalan/tanpa log,
// toggle langsung dari layar HP, lihat counter stack real-time.
// ---------------------------------------------------------------------------
namespace menu {

// Dipanggil sekali setelah il2cpp siap + gum init
bool Init();

// Dipanggil tiap frame (dari hook eglSwapBuffers)
void Render();

// Status untuk ditampilkan di menu
struct Status {
    bool il2cpp_ready   = false;
    bool gum_ready      = false;
    int  hooks_ok       = 0;
    int  hooks_total    = 6;
    int  stack_count    = 0;
    int  stack_target   = 14;
};

extern Status status;

} // namespace menu
