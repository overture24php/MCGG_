// ---------------------------------------------------------------------------
// Implementasi resolver IL2CPP.
// Tidak ada il2cpp_* yang di-link. APK game ini DIPAK:
//   - stub libil2cpp.so mengekspor thunk il2cpp_* (16 byte) + variabel gate
//     m_il2cpp_*_ptr yang diisi unpacker dengan alamat fungsi asli;
//   - lib asli (155 MB, mis. app_libs/liblogic.so) muncul di /proc/self/maps
//     dengan nama/mekanisme apa pun (rename, memfd, apk!/lib/...).
// Karena itu modul ditemukan lewat scan maps + parse ELF manual (GNU hash),
// bukan dlsym by soname — pendekatan yang sama dengan libTool (mlsmanXP).
// ---------------------------------------------------------------------------
#include "il2cpp.h"
#include "log.h"

#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <unistd.h>
#include <elf.h>
#include <initializer_list>

namespace il2 {

Api api{};
static void*  g_handle = nullptr;   // pin handle dlopen (boleh null kalau sumber dari maps)
static Image* g_cs     = nullptr;

// ---------------------------------------------------------------------------
// Trace diagnostik discovery: logcat (tag MCGGMOD) + file /sdcard/mcggmod_il2.txt
// supaya kegagalan bisa dianalisis walau buffer logcat sudah ke-rotate.
// ---------------------------------------------------------------------------
static void Trace(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
    FILE* f = std::fopen("/sdcard/mcggmod_il2.txt", "a");
    if (f) { std::fprintf(f, "%s\n", buf); std::fclose(f); }
}

// log hanya saat pesan berubah (anti-spam di loop polling)
static void TraceState(const char* msg) {
    static char last[160] = {0};
    if (std::strcmp(last, msg) == 0) return;
    std::strncpy(last, msg, sizeof(last) - 1);
    last[sizeof(last) - 1] = 0;
    Trace("%s", msg);
}

// ---------------------------------------------------------------------------
// Parser ELF manual by-memory (name-agnostic): parse ELF header + PT_DYNAMIC
// + GNU hash langsung dari alamat base modul di /proc/self/maps. Tidak peduli
// lib di-rename / di-load dari memfd / file-nya sudah (deleted) — pola packer.
// Pendekatan setara libTool (mlsmanXP) yang memakai solist linker + do_dlsym.
// ---------------------------------------------------------------------------
struct ModSyms {
    uintptr_t base = 0;
    const Elf64_Sym* symtab = nullptr;
    const char* strtab = nullptr;
    const uint32_t* buckets = nullptr;
    const uint32_t* chain = nullptr;
    uint32_t nbuckets = 0;
    uint32_t symoffset = 0;
    uint32_t nchain = 0;    // dari DT_HASH kalau ada; 0 = tidak diketahui
    std::string path;       // path di /proc/self/maps (diagnostik)
    bool ok = false;
};

static bool ParseModule(uintptr_t base, ModSyms& m) {
    const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(base);
    if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3)
        return false;
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) return false;
    if (eh->e_phoff == 0 || eh->e_phoff > 0x10000 || eh->e_phnum == 0 || eh->e_phnum > 64)
        return false;
    const auto* ph = reinterpret_cast<const Elf64_Phdr*>(base + eh->e_phoff);
    const Elf64_Dyn* dyn = nullptr;
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const Elf64_Dyn*>(base + ph[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return false;
    const uint32_t* gnu = nullptr;
    for (const Elf64_Dyn* d = dyn; d->d_tag != DT_NULL; d++) {
        if      (d->d_tag == DT_SYMTAB)   m.symtab = reinterpret_cast<const Elf64_Sym*>(base + d->d_un.d_ptr);
        else if (d->d_tag == DT_STRTAB)   m.strtab = reinterpret_cast<const char*>(base + d->d_un.d_ptr);
        else if (d->d_tag == DT_GNU_HASH) gnu = reinterpret_cast<const uint32_t*>(base + d->d_un.d_ptr);
        else if (d->d_tag == DT_HASH)     m.nchain = reinterpret_cast<const uint32_t*>(base + d->d_un.d_ptr)[1];
    }
    if (!m.symtab || !m.strtab || !gnu) return false;
    m.nbuckets  = gnu[0];
    m.symoffset = gnu[1];
    uint32_t bloom_size = gnu[2];
    if (m.nbuckets == 0 || m.nbuckets > (1u << 24)) return false;
    m.buckets = gnu + 4 + (bloom_size * 2);   // bloom filter = bloom_size * uint64
    m.chain   = m.buckets + m.nbuckets;
    m.base = base;
    m.ok = true;
    return true;
}

static uint32_t GnuHash(const char* s) {
    uint32_t h = 5381;
    for (; *s; s++) h = h * 33 + static_cast<uint8_t>(*s);
    return h;
}

static void* ModLookup(const ModSyms& m, const char* name) {
    if (!m.ok) return nullptr;
    uint32_t h = GnuHash(name);
    uint32_t i = m.buckets[h % m.nbuckets];
    if (i < m.symoffset) return nullptr;
    uint32_t maxChain = m.nchain ? (m.nchain - m.symoffset) : (1u << 22);
    for (uint32_t guard = 0; guard < maxChain; i++, guard++) {
        uint32_t hi = m.chain[i - m.symoffset];
        const Elf64_Sym& s = m.symtab[i];
        if (s.st_shndx != 0 && (hi | 1) == (h | 1) &&
            std::strcmp(m.strtab + s.st_name, name) == 0)
            return reinterpret_cast<void*>(m.base + s.st_value);
        if (hi & 1) break;
    }
    return nullptr;
}

static std::vector<ModSyms> g_mods;   // SEMUA modul yang mengekspor il2cpp:
                                      // stub gate libil2cpp.so + lib asli (hasil unpack)
static bool g_api_ok = false;         // true setelah ResolveAll() lengkap

// path sudah pernah di-trace? (anti-spam saat polling maps)
static bool SeenPath(const char* p) {
    static std::vector<std::string> seen;
    for (size_t i = 0; i < seen.size(); i++)
        if (seen[i] == p) return true;
    seen.push_back(p);
    return false;
}

// Uji satu kandidat base dari maps. Kandidat valid kalau punya gate
// m_il2cpp_*_ptr (pola stub Moonton) atau thunk il2cpp_* (Unity asli).
static bool ProbeModule(uintptr_t base, const char* path) {
    for (size_t i = 0; i < g_mods.size(); i++)
        if (g_mods[i].base == base) return false;
    ModSyms m;
    if (!ParseModule(base, m)) return false;
    const bool gate   = ModLookup(m, "m_il2cpp_domain_get_ptr") != nullptr;
    const bool direct = ModLookup(m, "il2cpp_domain_get") != nullptr;
    if (!gate && !direct) return false;
    m.path = path ? path : "";
    g_mods.push_back(m);
    Trace("modul il2cpp: base=%p gate=%d direct=%d path=%s",
          (void*)base, (int)gate, (int)direct, path ? path : "?");
    return true;
}

// Scan /proc/self/maps — teknik libTool (mlsmanXP): TIDAK bergantung nama
// file / namespace linker. Lib boleh di-rename, sudah (deleted), atau
// di-load dari memfd / apk!/lib/... — semua kelihatan di maps.
static void ScanMapsOnce() {
    // sekali saja (non-fatal): pin handle kalau linker masih mengenali soname
    // supaya lib tidak di-unload. NOLOAD = jangan memicu load baru.
    static bool pin_done = false;
    if (!pin_done) {
        pin_done = true;
        const char* names[] = {"libil2cpp.so", "liblogic.so", "libunity.so"};
        for (const char* n : names) {
            void* h = dlopen(n, RTLD_NOLOAD | RTLD_NOW);
            if (h) { g_handle = h; Trace("dlopen NOLOAD ok: %s", n); }
        }
        // path hasil unpacker: /data/user/0/<pkg>/app_libs/... (NOLOAD saja)
        char pkg[128] = {0};
        FILE* cf = std::fopen("/proc/self/cmdline", "r");
        if (cf) {
            if (std::fread(pkg, 1, sizeof(pkg) - 1, cf) == 0) pkg[0] = 0;
            std::fclose(cf);
        }
        if (char* c = std::strchr(pkg, ':')) *c = 0;
        if (pkg[0]) {
            const char* libs[] = {"liblogic.so", "libil2cpp.so", "libunity.so"};
            for (const char* ln : libs) {
                std::string p = std::string("/data/user/0/") + pkg + "/app_libs/" + ln;
                void* h = dlopen(p.c_str(), RTLD_NOLOAD | RTLD_NOW);
                if (h) { g_handle = h; Trace("dlopen NOLOAD ok: %s", p.c_str()); }
            }
        }
    }

    FILE* f = std::fopen("/proc/self/maps", "r");
    if (!f) { Trace("maps tidak terbuka"); return; }
    char line[768];
    while (std::fgets(line, sizeof(line), f)) {
        uintptr_t start = 0, end = 0, off = 0;
        char perms[8] = {0};
        if (std::sscanf(line, "%lx-%lx %7s %lx", &start, &end, perms, &off) != 4) continue;
        if (off != 0 || perms[0] != 'r') continue;   // hanya segmen pertama (header ELF)
        char* path = std::strchr(line, '/');
        if (path) { char* nl = std::strchr(path, '\n'); if (nl) *nl = 0; }
        if (path && (std::strstr(path, "liblogic") || std::strstr(path, "libil2cpp") ||
                     std::strstr(path, "libResources") || std::strstr(path, "memfd") ||
                     std::strstr(path, "apk!") || std::strstr(path, "libunity"))) {
            if (!SeenPath(path)) Trace("maps kandidat: %s base=%p", path, (void*)start);
        }
        ProbeModule(start, path);
    }
    std::fclose(f);
}

// Resolver tunggal untuk RESOLVE().
// 1) gate m_<nama>_ptr / m_<nama> (variabel 8 byte yang diisi unpacker)
//    -> kalau terisi, isinya alamat fungsi asli.
// 2) direct/thunk il2cpp_* HANYA dari modul yang tidak punya gate itu
//    (lib asli hasil unpack biasanya diekspor langsung).
// Gate ada tapi masih null = belum siap -> return null, Wait() mengulang.
static void* Sym(const char* s) {
    const std::string g1 = std::string("m_") + s + "_ptr";
    const std::string g2 = std::string("m_") + s;
    for (size_t i = 0; i < g_mods.size(); i++) {
        if (!g_mods[i].ok) continue;
        for (const std::string* g : {&g1, &g2}) {
            void* slot = ModLookup(g_mods[i], g->c_str());
            if (slot) {
                void* real = *reinterpret_cast<void**>(slot);
                if (real) return real;
            }
        }
    }
    for (size_t i = 0; i < g_mods.size(); i++) {
        if (!g_mods[i].ok) continue;
        if (ModLookup(g_mods[i], g1.c_str()) || ModLookup(g_mods[i], g2.c_str()))
            continue;   // gate ada tapi belum diisi -> lewati modul ini
        if (void* v = ModLookup(g_mods[i], s)) return v;
    }
    return nullptr;
}

#define RESOLVE(field, sym)                                              \
    do {                                                                 \
        api.field = reinterpret_cast<decltype(api.field)>(               \
            Sym(sym));                                                   \
        if (!api.field) { Trace("resolve GAGAL: %s", sym); return false; } \
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

// (Penemuan modul il2cpp sekarang lewat ScanMapsOnce() — bukan dlopen by
//  soname lagi: APK ini dipak, lib asli bisa di-rename / memfd / tidak
//  terdaftar di namespace linker.)

bool Wait(int timeout_ms) {
    const int step = 250;
    int waited = 0;

    for (;;) {
        // 1. temukan modul il2cpp lewat /proc/self/maps (stub gate +/atau
        //    lib hasil unpack). Tidak butuh dlopen by-name.
        if (!g_api_ok) ScanMapsOnce();

        // 2. resolve API. Gagal HANYA berarti gate m_*_ptr belum diisi packer
        //    -> ulangi di iterasi berikutnya, JANGAN menyerah (versi lama
        //    return false di sini = langsung FAILED walau cuma belum siap).
        if (!g_mods.empty() && !g_api_ok) {
            if (ResolveAll()) {
                g_api_ok = true;
                Trace("API il2cpp ter-resolve (%zu modul, %d ms)", g_mods.size(), waited);
            } else {
                TraceState("API belum lengkap (gate belum diisi / simbol belum ada)");
            }
        }

        // 3. metadata siap? (Assembly-CSharp.dll sudah terdaftar di domain)
        if (g_api_ok && LocateCsImage()) {
            Trace("il2cpp SIAP setelah %d ms", waited);
            return true;
        }

        if (timeout_ms >= 0 && waited >= timeout_ms) {
            Trace("TIMEOUT %d ms: modul=%zu api=%d", timeout_ms, g_mods.size(), (int)g_api_ok);
            return false;
        }
        usleep(step * 1000);
        waited += step;
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
