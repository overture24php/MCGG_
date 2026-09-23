// ---------------------------------------------------------------------------
// Implementasi resolver IL2CPP.
// Semua API di-resolve lewat dlsym("libil2cpp.so"), TIDAK di-link.
// Pola ini diambil dari cara libTool bekerja: import ELF-nya bersih
// (cuma pthread/fopen), semua il2cpp_* dicari saat runtime.
// ---------------------------------------------------------------------------
#include "il2cpp.h"
#include "log.h"

#include <dlfcn.h>
#include <cstring>
#include <unistd.h>

namespace il2 {

Api api{};
static void*  g_handle = nullptr;
static Image* g_cs     = nullptr;

#define RESOLVE(field, sym)                                              \
    do {                                                                 \
        api.field = reinterpret_cast<decltype(api.field)>(               \
            dlsym(g_handle, sym));                                       \
        if (!api.field) { LOGE("dlsym gagal: %s", sym); return false; }   \
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
    RESOLVE(type_get_class,            "il2cpp_type_get_class");
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

// Cari library il2cpp. Nama bisa berbeda antar game/build:
//   - libil2cpp.so (standar Unity)
//   - liblogic.so (beberapa build Moonton)
//   - libgame.so (beberapa embed langsung)
//   - libmoba.so (MLBB mobile)
// Polling semua nama sampai ketemu.
static const char* kLibNames[] = {
    "libil2cpp.so",
    "liblogic.so",
    "libgame.so",
    "libmoba.so",
    "libunity.so",
};

static bool TryLoadLib() {
    for (const char* name : kLibNames) {
        g_handle = dlopen(name, RTLD_NOLOAD | RTLD_NOW);
        if (!g_handle) g_handle = dlopen(name, RTLD_NOW);
        if (g_handle) {
            LOGI("il2cpp lib termuat: %s", name);
            return true;
        }
    }
    return false;
}

bool Wait(int timeout_ms) {
    const int step = 250;
    int waited = 0;

    for (;;) {
        if (!g_handle) {
            if (!TryLoadLib())
                LOGW("il2cpp lib BELUM termuat (waited %d ms)", waited);
        }

        if (g_handle && !api.domain_get) {
            if (!ResolveAll()) {
                LOGE("resolve API il2cpp gagal");
                return false;
            }
            LOGI("API il2cpp ter-resolve");
        }

        if (api.domain_get) {
            Domain* dom = api.domain_get();
            if (dom) {
                size_t n = 0;
                Assembly** list = api.domain_get_assemblies(dom, &n);
                LOGI("domain OK, %zu assembly terdaftar", n);
                if (list && n > 0) {
                    for (size_t i = 0; i < n && i < 10; i++) {
                        Image* img = api.assembly_get_image(list[i]);
                        if (!img) continue;
                        const char* nm = api.image_get_name(img);
                        LOGI("  [%zu] %s", i, nm ? nm : "(null)");
                    }
                }
            } else {
                LOGW("domain_get() return null");
            }
        }

        // metadata siap?
        if (api.domain_get && LocateCsImage()) {
            LOGI("il2cpp SIAP setelah %d ms", waited);
            return true;
        }

        if (timeout_ms >= 0 && waited >= timeout_ms) {
            LOGE("timeout %d ms: il2cpp/metadata tidak siap", timeout_ms);
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
