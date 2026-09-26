// ---------------------------------------------------------------------------
// Implementasi resolver IL2CPP (v3).
//
// PELAJARAN v2 (sigsetjmp "guard"): longjmp keluar dari tengah malloc/stdio saat
// packer mencabut halaman modul => thread mod wedged (trace berhenti total,
// status nyangkut "waiting_for_il2cpp"). v3: TIDAK ADA handler sinyal. Semua
// baca memori lewat process_vm_readv (gagal = EFAULT, bukan SIGSEGV).
//
// Fakta APK v1.2.98.3143 (terverifikasi di HP, 26-09-2026):
//   - libil2cpp.so = STUB 384 KB: export 243 gate m_il2cpp_*_ptr + thunk 16-byte
//     il2cpp_*; gate TIDAK PERNAH diisi packer versi ini -> penyebab "resolve
//     GAGAL" berulang di v2 (gate selalu null).
//   - il2cpp ASLI di-unpack ke /data/data/<pkg>/app_libs/liblogic.so (162 MB,
//     file nyata milik uid app) lalu di-mmap packer (custom loader, bukan linker).
//     File itu mengekspor il2cpp_* LANGSUNG tanpa gate.
// Urutan sumber symbol di v3:
//   1) dlopen(NOLOAD)+dlsym  -> benar kalau linker masih mengenal lib asli;
//   2) FILE app_libs/liblogic.so -> parse ELF dari FILE (pread, bebas fault),
//      base runtime dicari di /proc/self/maps;
//   3) gate m_<nama>_ptr dari modul di memori (kalau packer mengisinya);
//   4) symbol langsung di modul memori (lib asli kalau ter-mapping terpisah).
// ---------------------------------------------------------------------------
#include "il2cpp.h"
#include "log.h"

#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <time.h>
#include <initializer_list>
#include <sys/syscall.h>
#include <sys/uio.h>

#ifndef __NR_process_vm_readv
#define __NR_process_vm_readv 270   // arm64
#endif

namespace il2 {

Api api{};
static void*  g_handle = nullptr;   // handle dlopen (pin + sumber dlsym)
static Image* g_cs     = nullptr;

static long NowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Baca memori proses sendiri TANPA bisa crash (EFAULT, bukan SIGSEGV).
static bool MemRead(uintptr_t addr, void* dst, size_t n) {
    struct iovec loc { dst, n };
    struct iovec rem { reinterpret_cast<void*>(addr), n };
    long r = syscall(__NR_process_vm_readv, (long)getpid(), &loc, 1L, &rem, 1L, 0L);
    return r == (long)n;
}

// ---- identitas proses (nama file trace) ----
static const char* ProcName() {
    static char buf[128] = "?";
    static bool done = false;
    if (!done) {
        done = true;
        int fd = open("/proc/self/cmdline", O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = 0;
                for (ssize_t i = 0; i < n; i++) if (buf[i] == 0) buf[i] = ' ';
            }
            close(fd);
        }
    }
    return buf;
}
static const char* PkgName() {                       // cmdline tanpa ":suffix"
    static char buf[128] = {0};
    static bool done = false;
    if (!done) {
        done = true;
        std::snprintf(buf, sizeof(buf), "%s", ProcName());
        if (char* c = std::strchr(buf, ':')) *c = 0;
        if (char* c = std::strchr(buf, ' ')) *c = 0;
    }
    return buf;
}

// Trace: logcat + file app-private (ext4, cepat). Prefix [pid t=ms] supaya dua
// proses (:UnityKillsMe vs shell/extractor) tidak tertukar saat file dibaca.
static void Trace(const char* fmt, ...) {
    char msg[400];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    static long t0 = NowMs();
    char line[480];
    std::snprintf(line, sizeof(line), "[%d t=%ld] %s", (int)getpid(), NowMs() - t0, msg);
    LOGI("%s", line);
    char path[200];
    std::snprintf(path, sizeof(path), "/data/data/%s/files/mcggmod_il2_%d.txt",
                  PkgName(), (int)getpid());
    FILE* f = std::fopen(path, "a");
    if (f) { std::fprintf(f, "%s\n", line); std::fclose(f); }
}

// log hanya saat pesan berubah (anti-spam di loop polling)
static void TraceState(const char* msg) {
    static char last[224] = {0};
    if (std::strcmp(last, msg) == 0) return;
    std::strncpy(last, msg, sizeof(last) - 1);
    last[sizeof(last) - 1] = 0;
    Trace("%s", msg);
}

// ---------------------------------------------------------------------------
// Pembaca dynsym ELF v3. SENGAJA tanpa GNU hash: dynsym il2cpp kecil (validator:
// liblogic.so = 2.837 symbol, stub = 1.084 symbol) -> salin sekali lalu scan
// linear. Dua sumber baca:
//   - FILE   (app_libs/liblogic.so): pread per-offset, sama sekali bebas fault;
//   - MEMORI (modul di /proc/self/maps): process_vm_readv (EFAULT, bukan SEGV).
// ---------------------------------------------------------------------------
struct ElfMod {
    bool        in_file = false;
    int         fd = -1;
    uintptr_t   base = 0;        // mem: base langsung; file: diisi dari maps
    std::string path;

    struct Seg { uint64_t off, vaddr, filesz; };
    std::vector<Seg>       loads;   // PT_LOAD (konversi vaddr -> offset file)
    std::vector<Elf64_Sym> syms;    // salinan dynsym
    std::vector<char>      strtab;  // salinan dynstr (selalu diakhiri NUL)

    bool ok = false;
    bool has_gate_var = false;      // punya export m_il2cpp_domain_get_ptr (stub)
};

// Baca `n` byte dari vaddr modul (file: pread; memori: process_vm_readv).
static bool ReadV(const ElfMod& m, uint64_t vaddr, void* dst, size_t n) {
    size_t got = 0;
    char* out = static_cast<char*>(dst);
    while (got < n) {
        uint64_t v = vaddr + got;
        if (m.in_file) {
            uint64_t off = 0, room = 0;
            for (const auto& s : m.loads) {
                if (v >= s.vaddr && v < s.vaddr + s.filesz) {
                    off  = s.off + (v - s.vaddr);
                    room = s.vaddr + s.filesz - v;
                    break;
                }
            }
            if (!room) return false;
            size_t chunk = (n - got < room) ? (n - got) : (size_t)room;
            ssize_t r = pread(m.fd, out + got, chunk, (off_t)off);
            if (r <= 0) return false;
            got += (size_t)r;
        } else {
            size_t chunk = (n - got < 4096) ? (n - got) : 4096;
            if (!MemRead(m.base + v, out + got, chunk)) return false;
            got += chunk;
        }
    }
    return true;
}

// Baca pointer 8 byte di vaddr modul (gate m_<nama>_ptr pola stub).
static bool ReadPtrAt(const ElfMod& m, uint64_t vaddr, void** out) {
    uintptr_t p = 0;
    if (!ReadV(m, vaddr, &p, sizeof(p))) return false;
    *out = reinterpret_cast<void*>(p);
    return true;
}

// Muat header + PT_LOAD/PT_DYNAMIC + salin dynsym & dynstr ke buffer sendiri.
// Setelah ini modul tidak disentuh lagi (aman walau packer me-remap halamannya).
static bool ElfLoad(ElfMod& m) {
    Elf64_Ehdr eh{};
    if (m.in_file) {
        if (pread(m.fd, &eh, sizeof(eh), 0) != (ssize_t)sizeof(eh)) return false;
    } else {
        if (!MemRead(m.base, &eh, sizeof(eh))) return false;
    }
    if (std::memcmp(eh.e_ident, ELFMAG, 4) != 0) return false;
    if (eh.e_ident[EI_CLASS] != ELFCLASS64) return false;
    if (!eh.e_phoff || !eh.e_phnum || eh.e_phnum > 512) return false;

    std::vector<Elf64_Phdr> ph(eh.e_phnum);
    const size_t ph_bytes = ph.size() * sizeof(Elf64_Phdr);
    if (m.in_file) {
        if (pread(m.fd, ph.data(), ph_bytes, (off_t)eh.e_phoff) != (ssize_t)ph_bytes)
            return false;
    } else {
        if (!MemRead(m.base + eh.e_phoff, ph.data(), ph_bytes)) return false;
    }

    uint64_t dyn_v = 0, dyn_sz = 0, gnu_v = 0;
    for (const auto& p : ph) {
        if (p.p_type == PT_LOAD && p.p_filesz)
            m.loads.push_back({p.p_offset, p.p_vaddr, p.p_filesz});
        if (p.p_type == PT_DYNAMIC && !dyn_v) { dyn_v = p.p_vaddr; dyn_sz = p.p_filesz; }
    }
    if (!dyn_v || m.loads.empty()) return false;
    if (dyn_sz == 0 || dyn_sz > 0x10000) dyn_sz = 0x10000;

    std::vector<Elf64_Dyn> dyn((size_t)(dyn_sz / sizeof(Elf64_Dyn)));
    if (!ReadV(m, dyn_v, dyn.data(), dyn.size() * sizeof(Elf64_Dyn))) return false;

    uint64_t sym_v = 0, str_v = 0, strsz = 0, hash_v = 0;
    for (const auto& d : dyn) {
        if (d.d_tag == DT_NULL) break;
        if      (d.d_tag == DT_SYMTAB)   sym_v  = d.d_un.d_ptr;
        else if (d.d_tag == DT_STRTAB)   str_v  = d.d_un.d_ptr;
        else if (d.d_tag == DT_STRSZ)    strsz  = d.d_un.d_ptr;
        else if (d.d_tag == DT_HASH)     hash_v = d.d_un.d_ptr;
        else if (d.d_tag == DT_GNU_HASH) gnu_v  = d.d_un.d_ptr;
    }
    if (!sym_v || !str_v) return false;

    uint32_t count = 0;
    if (hash_v) {                       // DT_HASH -> nchain = jumlah symbol
        uint32_t nh[2] = {0, 0};
        if (ReadV(m, hash_v, nh, sizeof(nh))) count = nh[1];
    }
    if (!count && gnu_v) {              // turunkan batas jumlah symbol dari GNU hash
        uint32_t gh[4] = {0, 0, 0, 0};
        if (ReadV(m, gnu_v, gh, sizeof(gh)) && gh[0] && gh[0] <= 0x10000) {
            uint64_t buckets_v = gnu_v + 16 + (uint64_t)gh[2] * 8;
            std::vector<uint32_t> bk(gh[0]);
            if (ReadV(m, buckets_v, bk.data(), bk.size() * 4)) {
                uint32_t mx = 0;
                for (uint32_t b : bk) if (b > mx) mx = b;
                if (mx >= gh[1]) {
                    for (uint32_t idx = mx, hops = 0; hops < 0x40000; idx++, hops++) {
                        uint32_t w = 0;
                        if (!ReadV(m, buckets_v + (uint64_t)gh[0] * 4 +
                                          (uint64_t)(idx - gh[1]) * 4, &w, 4)) break;
                        if (w & 1) { count = idx + 1; break; }
                    }
                }
            }
        }
    }
    if (!count && str_v > sym_v)
        count = (uint32_t)((str_v - sym_v) / sizeof(Elf64_Sym));
    if (!count || count > 0x40000) count = 0x40000;    // batas aman

    if (!strsz || strsz > (2u << 20)) strsz = 1u << 20;

    // Baca dynsym; kalau range-nya menyentuh halaman tak terpetakan (memori),
    // perkecil sampai berhasil (tidak pernah crash: ReadV aman).
    m.syms.resize(count);
    size_t want = m.syms.size();
    while (want > 64 && !ReadV(m, sym_v, m.syms.data(), want * sizeof(Elf64_Sym)))
        want /= 2;
    if (want <= 64) return false;
    m.syms.resize(want);

    m.strtab.resize((size_t)strsz + 1);
    size_t swant = (size_t)strsz;
    while (swant > 256 && !ReadV(m, str_v, m.strtab.data(), swant)) swant /= 2;
    if (swant <= 256) return false;
    m.strtab.resize(swant + 1);
    m.strtab[swant] = 0;

    m.ok = true;
    return true;
}

// Cari symbol DEFINED by nama (scan linear; tabelnya kecil, sekali muat).
static bool FindSym(const ElfMod& m, const char* name, uint64_t* val) {
    if (!m.ok) return false;
    for (const auto& s : m.syms) {
        if (!s.st_shndx || !s.st_name || s.st_name >= m.strtab.size()) continue;
        if (std::strcmp(&m.strtab[s.st_name], name) == 0) {
            if (val) *val = s.st_value;
            return true;
        }
    }
    return false;
}

static std::vector<ElfMod> g_mods;    // semua sumber symbol yang ditemukan
static bool g_api_ok = false;         // true setelah ResolveAll() lengkap

// Range executable dari maps terakhir (validasi hasil resolve).
static std::vector<std::pair<uintptr_t, uintptr_t>> g_exec;
static bool IsExecAddr(uintptr_t a) {
    for (const auto& r : g_exec) if (a >= r.first && a < r.second) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Discovery sumber symbol:
//   A. FILE app_libs/liblogic.so — il2cpp asli hasil unpack (paling diandalkan);
//   B. dlopen(NOLOAD)+dlsym   — bila linker masih mengenal lib aslinya;
//   C. modul di /proc/self/maps — pola umum (stub / lib lain / mapping anonim).
// ---------------------------------------------------------------------------
static bool AddFileMod(const char* path) {
    for (const auto& e : g_mods)
        if (e.in_file && e.path == path) return true;   // sudah ada
    ElfMod m;
    m.in_file = true;
    m.path = path;
    m.fd = open(path, O_RDONLY | O_CLOEXEC);
    if (m.fd < 0) return false;
    if (!ElfLoad(m)) {
        close(m.fd);
        return false;
    }
    m.has_gate_var = FindSym(m, "m_il2cpp_domain_get_ptr", nullptr);
    const bool direct = FindSym(m, "il2cpp_domain_get", nullptr);
    if (!direct && !m.has_gate_var) {
        close(m.fd);
        return false;
    }
    Trace("modul FILE: %s (%zu symbol, direct=%d gate=%d) — base menyusul dari maps",
          path, m.syms.size(), (int)direct, (int)m.has_gate_var);
    g_mods.push_back(std::move(m));
    return true;
}

static bool AddMemMod(uintptr_t base, const char* path) {
    for (const auto& e : g_mods)
        if (!e.in_file && e.base == base) return true;  // sudah pernah diprobe
    ElfMod m;
    m.base = base;
    m.path = path ? path : "";
    if (!ElfLoad(m)) return false;
    m.has_gate_var = FindSym(m, "m_il2cpp_domain_get_ptr", nullptr);
    const bool direct = FindSym(m, "il2cpp_domain_get", nullptr);
    if (!direct && !m.has_gate_var) return false;
    Trace("modul MAP: base=%p gate=%d direct=%d path=%s",
          (void*)base, (int)m.has_gate_var, (int)direct,
          (path && *path) ? path : "(anon)");
    g_mods.push_back(std::move(m));
    return true;
}

// dlopen(NOLOAD) saja — JANGAN memicu load baru (lib 162 MB; hindari init ganda).
// Handle diterima HANYA kalau bukan pola stub (tanpa gate m_il2cpp_domain_get_ptr):
// thunk stub belum terisi sampai packer selesai -> memakainya = crash.
static void TryDlopenOnce() {
    static int tries = 0;
    if (g_handle || tries >= 40) return;
    tries++;
    static std::string p[4];
    p[0] = std::string("/data/data/") + PkgName() + "/app_libs/liblogic.so";
    p[1] = std::string("/data/user/0/") + PkgName() + "/app_libs/liblogic.so";
    p[2] = std::string("/data/data/") + PkgName() + "/app_libs/libil2cpp.so";
    p[3] = std::string("/data/user/0/") + PkgName() + "/app_libs/libil2cpp.so";
    const char* cands[6] = {"liblogic.so", "libil2cpp.so",
                            p[0].c_str(), p[1].c_str(), p[2].c_str(), p[3].c_str()};
    for (const char* c : cands) {
        void* h = dlopen(c, RTLD_NOLOAD | RTLD_NOW);
        if (!h) continue;
        if (dlsym(h, "m_il2cpp_domain_get_ptr")) continue;   // itu stub (gate)
        if (!dlsym(h, "il2cpp_domain_get")) continue;
        g_handle = h;
        Trace("dlopen NOLOAD OK: %s (dlsym jadi sumber utama)", c);
        return;
    }
}

// path sudah pernah diprobe? (anti-spam saat polling maps)
static bool SeenPath(const char* p) {
    static std::vector<std::string> seen;
    for (const auto& s : seen) if (s == p) return true;
    seen.push_back(p);
    return false;
}

// Hanya modul relevan: /data (termasuk app_libs & apk!/lib/...), memfd, file
// (deleted). /system, /apex, /vendor, /dev dst tidak pernah memuat il2cpp.
static bool RelevantPath(const char* p) {
    if (!p || !*p) return false;
    if (std::strstr(p, "/memfd:") || std::strstr(p, "(deleted)")) return true;
    return std::strncmp(p, "/data/", 6) == 0;
}

// Base runtime modul FILE (liblogic di app_libs): mapping pertama (offset 0)
// dengan path yang cocok. Dipanggil tiap scan supaya base ikut pindah kalau
// packer me-remap.
static bool MatchFileBase(const char* map_path, uintptr_t start) {
    const char* suf = std::strstr(map_path, "/app_libs/");
    if (!suf) return false;
    for (auto& m : g_mods) {
        if (!m.in_file) continue;
        if (std::strstr(m.path.c_str(), suf)) {
            if (m.base != start) {
                m.base = start;
                Trace("base %s = %p (dari maps)", suf, (void*)start);
            }
            return true;
        }
    }
    return false;
}

// Scan /proc/self/maps: base modul FILE + probe modul memori yang belum dikenal.
static void ScanMaps() {
    FILE* f = std::fopen("/proc/self/maps", "r");
    if (!f) { Trace("maps tidak terbuka"); return; }
    g_exec.clear();
    char line[768];
    while (std::fgets(line, sizeof(line), f)) {
        uintptr_t start = 0, end = 0, off = 0;
        char perms[8] = {0};
        if (std::sscanf(line, "%lx-%lx %7s %lx", &start, &end, perms, &off) != 4) continue;
        if (perms[2] == 'x' && g_exec.size() < 1024)
            g_exec.push_back(std::make_pair(start, end));
        if (off != 0) continue;                      // hanya awal modul (header ELF)
        char* path = std::strchr(line, '/');
        if (path) { char* nl = std::strchr(path, '\n'); if (nl) *nl = 0; }
        if (path && MatchFileBase(path, start)) continue;
        if (path && !RelevantPath(path)) continue;   // /system, /apex, /dev, ...
        if (!path && perms[2] != 'x') continue;      // anonim: hanya kalau exec
        if (path && SeenPath(path)) continue;
        AddMemMod(start, path);
    }
    std::fclose(f);
}

// Satu putaran discovery (dipanggil Wait tiap ~1 detik).
static void DiscoverOnce() {
    static bool file_done = false;
    if (!file_done) {
        char p[220];
        std::snprintf(p, sizeof(p), "/data/data/%s/app_libs/liblogic.so", PkgName());
        if (AddFileMod(p)) {
            file_done = true;
        } else {
            std::snprintf(p, sizeof(p), "/data/user/0/%s/app_libs/liblogic.so", PkgName());
            if (AddFileMod(p)) file_done = true;
        }
    }
    TryDlopenOnce();
    ScanMaps();
}

// Resolver symbol (urutan prioritas v3 — lihat komentar atas file).
static void* Sym(const char* s) {
    // 1) dlsym bila linker mengenal lib asli
    if (g_handle) {
        if (void* p = dlsym(g_handle, s)) return p;
    }
    // 2) modul FILE (app_libs/liblogic.so): base runtime + st_value
    for (auto& m : g_mods) {
        if (!m.in_file || !m.base) continue;
        uint64_t v = 0;
        if (FindSym(m, s, &v) && v) return reinterpret_cast<void*>(m.base + v);
    }
    // 3) gate m_<nama>_ptr dari modul memori (pola stub, kalau packer mengisi)
    char g1[96], g2[96];
    std::snprintf(g1, sizeof(g1), "m_%s_ptr", s);
    std::snprintf(g2, sizeof(g2), "m_%s", s);
    for (auto& m : g_mods) {
        if (m.in_file || !m.ok) continue;
        uint64_t v = 0;
        if ((FindSym(m, g1, &v) || FindSym(m, g2, &v)) && v) {
            void* real = nullptr;
            if (ReadPtrAt(m, v, &real) && real) return real;
        }
    }
    // 4) symbol langsung dari modul memori TANPA gate
    for (auto& m : g_mods) {
        if (m.in_file || !m.ok || m.has_gate_var) continue;
        uint64_t v = 0;
        if (FindSym(m, s, &v) && v) return reinterpret_cast<void*>(m.base + v);
    }
    return nullptr;
}

#define RESOLVE(field, sym)                                              \
    do {                                                                 \
        api.field = reinterpret_cast<decltype(api.field)>(               \
            Sym(sym));                                                   \
        if (!api.field) { TraceState("resolve GAGAL: " sym); return false; } \
    } while (0)

static bool ResolveAll() {
    RESOLVE(domain_get,                "il2cpp_domain_get");
    RESOLVE(domain_get_assemblies,     "il2cpp_domain_get_assemblies");
    RESOLVE(assembly_get_image,        "il2cpp_assembly_get_image");
    RESOLVE(image_get_name,            "il2cpp_image_get_name");
    RESOLVE(class_from_name,           "il2cpp_class_from_name");
    RESOLVE(class_get_method_from_name,"il2cpp_class_get_method_from_name");
    RESOLVE(class_get_field_from_name, "il2cpp_class_get_field_from_name");
    RESOLVE(field_static_get_value,    "il2cpp_field_static_get_value");
    RESOLVE(field_static_set_value,    "il2cpp_field_static_set_value");
    RESOLVE(field_get_offset,          "il2cpp_field_get_offset");
    RESOLVE(thread_attach,             "il2cpp_thread_attach");
    RESOLVE(thread_detach,             "il2cpp_thread_detach");
    RESOLVE(class_get_name,            "il2cpp_class_get_name");
    RESOLVE(object_new,                "il2cpp_object_new");
    RESOLVE(runtime_invoke,            "il2cpp_runtime_invoke");
    RESOLVE(class_get_parent,          "il2cpp_class_get_parent");
    RESOLVE(class_get_methods,         "il2cpp_class_get_methods");
    RESOLVE(method_get_name,           "il2cpp_method_get_name");
    RESOLVE(method_get_param_count,    "il2cpp_method_get_param_count");
    RESOLVE(method_get_param,          "il2cpp_method_get_param");
    // il2cpp_type_get_class kadang tidak diekspor; Il2CppType.data.klass ada
    // di offset 0 -> shim aman selama pointer Il2CppType valid.
    api.type_get_class = reinterpret_cast<decltype(api.type_get_class)>(
        Sym("il2cpp_type_get_class"));
    if (!api.type_get_class)
        api.type_get_class = [](const void* t) -> Class* {
            return t ? *reinterpret_cast<Class* const*>(t) : nullptr;
        };
    // Sanity: alamat domain_get harus di region executable (kalau maps ter-scan).
    if (!g_exec.empty() &&
        !IsExecAddr(reinterpret_cast<uintptr_t>(api.domain_get))) {
        Trace("domain_get=%p bukan alamat exec — tolak (base salah?)",
              (void*)api.domain_get);
        api = Api{};
        return false;
    }
    return true;
}
#undef RESOLVE

// Cari Assembly-CSharp.dll di daftar assembly domain.
// Ini SEKALIGUS bukti metadata sudah terdekripsi: kalau packer belum
// selesai, domain_get() null atau daftar assembly kosong.
static bool LocateCsImage() {
    Domain* dom = api.domain_get ? api.domain_get() : nullptr;
    if (!dom) return false;

    ScopedThread st;   // attach dulu sebelum menyentuh domain

    size_t n = 0;
    Assembly** list = api.domain_get_assemblies(dom, &n);
    if (!list || n == 0) return false;

    for (size_t i = 0; i < n; i++) {
        Image* img = api.assembly_get_image(list[i]);
        if (!img) continue;
        const char* nm = api.image_get_name(img);
        if (nm && std::strcmp(nm, "Assembly-CSharp.dll") == 0) {
            g_cs = img;
            LOGI("Assembly-CSharp.dll ditemukan (%zu assembly total)", n);
            return true;
        }
    }
    LOGW("Assembly-CSharp.dll belum ada di %zu assembly", n);
    return false;
}

// (Discovery lewat DiscoverOnce(): file app_libs/liblogic.so -> dlopen NOLOAD ->
//  scan /proc/self/maps. Tidak ada lagi handler sinyal: semua baca via pread /
//  process_vm_readv, jadi tak mungkin wedged seperti versi sigsetjmp.)

bool Wait(int timeout_ms) {
    const int step = 250;
    int waited = 0;
    int tick = 0;
    const long t_start = NowMs();

    for (;;) {
        if ((tick % 4) == 0) {          // discovery + resolve tiap ~1 dtk
            if (!g_api_ok) {
                DiscoverOnce();
                if (ResolveAll()) {
                    g_api_ok = true;
                    Trace("API il2cpp ter-resolve (%zu sumber, %ld ms)",
                          g_mods.size(), NowMs() - t_start);
                } else {
                    TraceState("API belum lengkap (tunggu file/base/gate)");
                }
            }
            // metadata siap? (Assembly-CSharp.dll sudah terdaftar di domain)
            if (g_api_ok && LocateCsImage()) {
                Trace("il2cpp SIAP setelah %ld ms", NowMs() - t_start);
                return true;
            }
        }

        usleep(step * 1000);
        waited += step;
        tick++;
        if (timeout_ms >= 0 && waited >= timeout_ms) {
            Trace("TIMEOUT %d ms: sumber=%zu api=%d", timeout_ms,
                  g_mods.size(), (int)g_api_ok);
            return false;
        }
    }
}

Image* CsImage() { return g_cs; }

ScopedThread::ScopedThread() {
    if (api.thread_attach && api.domain_get) t = api.thread_attach(api.domain_get());
}
ScopedThread::~ScopedThread() {
    if (t && api.thread_detach) api.thread_detach(t);
}

Class* FindClass(const char* ns, const char* name) {
    if (!g_cs || !api.class_from_name) return nullptr;
    Class* k = api.class_from_name(g_cs, ns ? ns : "", name);
    if (!k) LOGW("class TIDAK ADA: %s%s%s", ns && *ns ? ns : "", ns && *ns ? "." : "", name);
    return k;
}

// ---- cari image lain (UnityEngine.CoreModule.dll, dsb) ----
Image* FindImage(const char* name) {
    if (!api.domain_get || !name) return nullptr;
    Domain* dom = api.domain_get();
    if (!dom) return nullptr;
    ScopedThread st;
    size_t n = 0;
    Assembly** list = api.domain_get_assemblies(dom, &n);
    if (!list) return nullptr;
    for (size_t i = 0; i < n; i++) {
        Image* img = api.assembly_get_image(list[i]);
        if (!img) continue;
        const char* nm = api.image_get_name(img);
        if (nm && std::strcmp(nm, name) == 0) return img;
    }
    LOGW("image TIDAK ADA: %s", name);
    return nullptr;
}

Class* FindClassIn(Image* img, const char* ns, const char* name) {
    if (!img || !api.class_from_name) return nullptr;
    Class* k = api.class_from_name(img, ns ? ns : "", name);
    if (!k) LOGW("class TIDAK ADA di image: %s%s%s", ns && *ns ? ns : "", ns && *ns ? "." : "", name);
    return k;
}

// ---- walk parent: il2cpp_class_get_method/field_from_name TIDAK menjamin
//      menemukan anggota yang dideklarasikan di base class (Singleton<T>,
//      MCLogicFighter, ...) -> walk manual pakai class_get_parent. ----
static Method* FindMethodOwn(Class* k, const char* name, int argc) {
    if (!k || !api.class_get_method_from_name) return nullptr;
    return api.class_get_method_from_name(k, name, argc);
}

M MethodFind(Class* k, const char* name, int argc) {
    M r;
    for (Class* c = k; c; c = api.class_get_parent ? api.class_get_parent(c) : nullptr) {
        Method* m = FindMethodOwn(c, name, argc);
        if (m) { r.mi = m; r.fn = *reinterpret_cast<void**>(m); break; }
    }
    if (!r.mi) {
        LOGW("method TIDAK ADA: %s (argc=%d)", name, argc);
    } else if (!r.fn) {
        LOGW("method %s ada tapi methodPointer null", name);
        r.mi = nullptr;
    }
    return r;
}

M MethodFind(const char* ns, const char* cls, const char* name, int argc) {
    return MethodFind(FindClass(ns, cls), name, argc);
}

// Membedakan overload via nama class arg ke-0 (SdpPacker vs SdpUnpacker).
M MethodFindArg0(Class* k, const char* name, int argc, const char* arg0Class) {
    M r;
    if (!k || !api.class_get_methods || !api.method_get_name ||
        !api.method_get_param_count || !api.method_get_param || !api.type_get_class) {
        LOGE("MethodFindArg0: API resolver belum lengkap");
        return r;
    }
    for (Class* c = k; c; c = api.class_get_parent ? api.class_get_parent(c) : nullptr) {
        void* iter = nullptr;
        while (Method* m = api.class_get_methods(c, &iter)) {
            const char* mn = api.method_get_name(m);
            if (!mn || std::strcmp(mn, name) != 0) continue;
            if ((int)api.method_get_param_count(m) != argc) continue;
            const void* t = api.method_get_param(m, 0);
            Class* pc = t ? api.type_get_class(t) : nullptr;
            const char* pn = pc ? api.class_get_name(pc) : nullptr;
            if (pn && std::strcmp(pn, arg0Class) == 0) {
                r.mi = m;
                r.fn = *reinterpret_cast<void**>(m);
                if (!r.fn) { LOGW("method %s(%s) methodPointer null", name, arg0Class); r.mi = nullptr; }
                goto done;
            }
        }
    }
done:
    if (!r.mi)
        LOGW("method TIDAK ADA: %s arg0=%s (argc=%d)", name, arg0Class, argc);
    return r;
}

void* MethodPtr(Class* k, const char* name, int argc) {
    return MethodFind(k, name, argc).fn;
}

void* MethodPtr(const char* ns, const char* cls, const char* name, int argc) {
    return MethodPtr(FindClass(ns, cls), name, argc);
}

Field* FieldFind(Class* k, const char* name) {
    if (!k || !api.class_get_field_from_name) return nullptr;
    for (Class* c = k; c; c = api.class_get_parent ? api.class_get_parent(c) : nullptr) {
        Field* f = api.class_get_field_from_name(c, name);
        if (f) return f;
    }
    LOGW("field TIDAK ADA: %s", name);
    return nullptr;
}

size_t FieldOffset(Class* k, const char* name) {
    Field* f = FieldFind(k, name);
    if (!f) return 0;
    return api.field_get_offset(f);
}

size_t FieldOffset(const char* ns, const char* cls, const char* name) {
    return FieldOffset(FindClass(ns, cls), name);
}

bool StaticGet(const char* ns, const char* cls, const char* field, void* out) {
    Class* k = FindClass(ns, cls);
    if (!k) return false;
    Field* f = FieldFind(k, field);
    if (!f) return false;
    api.field_static_get_value(f, out);
    return true;
}

bool StaticSet(const char* ns, const char* cls, const char* field, void* val) {
    Class* k = FindClass(ns, cls);
    if (!k) return false;
    Field* f = FieldFind(k, field);
    if (!f) return false;
    api.field_static_set_value(f, val);
    return true;
}

void* NewObject(const char* ns, const char* cls) {
    Class* k = FindClass(ns, cls);
    if (!k || !api.object_new) return nullptr;
    void* obj = api.object_new(k);
    if (!obj) return nullptr;
    // panggil .ctor() supaya field terinisialisasi (List, dll).
    // generated IL2CPP: .ctor(this, const MethodInfo*) -> kirim MethodInfo*.
    M ctor = MethodFind(k, ".ctor", 0);
    if (ctor.ok())
        reinterpret_cast<void (*)(void*, void*)>(ctor.fn)(obj, ctor.mi);
    return obj;
}

} // namespace il2
