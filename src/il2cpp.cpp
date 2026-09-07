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

bool Wait(int timeout_ms) {
    const int step = 250;
    int waited = 0;

    for (;;) {
        if (!g_handle) {
            g_handle = dlopen("libil2cpp.so", RTLD_NOLOAD | RTLD_NOW);
            if (!g_handle) g_handle = dlopen("libil2cpp.so", RTLD_NOW);
            if (g_handle) LOGI("libil2cpp.so termuat");
            else LOGW("libil2cpp.so BELUM termuat (waited %d ms)", waited);
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

void* MethodPtr(Class* k, const char* name, int argc) {
    if (!k || !api.class_get_method_from_name) return nullptr;
    Method* m = api.class_get_method_from_name(k, name, argc);
    if (!m) { LOGW("method TIDAK ADA: %s (argc=%d)", name, argc); return nullptr; }
    // MethodInfo layout: field pertama = methodPointer (native code)
    void* p = *reinterpret_cast<void**>(m);
    if (!p) LOGW("method %s ada tapi methodPointer null", name);
    return p;
}

void* MethodPtr(const char* ns, const char* cls, const char* name, int argc) {
    return MethodPtr(FindClass(ns, cls), name, argc);
}

size_t FieldOffset(Class* k, const char* name) {
    if (!k || !api.class_get_field_from_name) return 0;
    Field* f = api.class_get_field_from_name(k, name);
    if (!f) { LOGW("field TIDAK ADA: %s", name); return 0; }
    return api.field_get_offset(f);
}

size_t FieldOffset(const char* ns, const char* cls, const char* name) {
    return FieldOffset(FindClass(ns, cls), name);
}

bool StaticGet(const char* ns, const char* cls, const char* field, void* out) {
    Class* k = FindClass(ns, cls);
    if (!k) return false;
    Field* f = api.class_get_field_from_name(k, field);
    if (!f) return false;
    api.field_static_get_value(f, out);
    return true;
}

bool StaticSet(const char* ns, const char* cls, const char* field, void* val) {
    Class* k = FindClass(ns, cls);
    if (!k) return false;
    Field* f = api.class_get_field_from_name(k, field);
    if (!f) return false;
    api.field_static_set_value(f, val);
    return true;
}

void* NewObject(const char* ns, const char* cls) {
    Class* k = FindClass(ns, cls);
    if (!k || !api.object_new) return nullptr;
    void* obj = api.object_new(k);
    if (!obj) return nullptr;
    // panggil .ctor() supaya field terinisialisasi (List, dll)
    void* ctor = MethodPtr(k, ".ctor", 0);
    if (ctor) reinterpret_cast<void (*)(void*)>(ctor)(obj);
    return obj;
}

} // namespace il2
