#pragma once
// ---------------------------------------------------------------------------
// Wrapper tipis di atas frida-gum interceptor.
//
// Kenapa gum: pola yang sama dipakai libTool (ptr_inject_ToolVIP.sh) — simbol
// Gum::Interceptor / GumInvocationListener terlihat di ELF-nya. Gum statis
// artinya TIDAK butuh frida-server, jadi port 27042 tidak pernah terbuka.
//
// Semua fitur kita cukup ATTACH (observasi + ubah field lewat pointer `this`),
// tidak perlu REPLACE. Itu jauh lebih aman: alur asli tetap jalan.
// ---------------------------------------------------------------------------
#include <cstdint>

namespace hook {

// Callback dipanggil sebelum fungsi asli jalan.
// args[0] = `this` untuk instance method (arg ke-0 di arm64 = x0).
using EnterFn = void (*)(void** args);
using LeaveFn = void (*)(void** args, void* retval);

bool Init();

// Pasang hook di alamat native. Salah satu callback boleh null.
// label hanya untuk log.
bool Attach(void* addr, EnterFn on_enter, LeaveFn on_leave, const char* label);

} // namespace hook
