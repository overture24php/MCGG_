// ---------------------------------------------------------------------------
// Implementasi hook di atas frida-gum.
// Dipakai mode ATTACH (bukan replace) supaya alur asli game tetap jalan.
// ---------------------------------------------------------------------------
#include "hook.h"
#include "log.h"

#include <frida-gum.h>
#include <cstdlib>
#include <cstring>

namespace hook {

// listener kustom: satu instance per hook, menyimpan pasangan callback
typedef struct _McggListener McggListener;

struct _McggListener {
    GObject parent;
    EnterFn on_enter;
    LeaveFn on_leave;
    const char* label;
};

static void mcgg_listener_iface_init(gpointer g_iface, gpointer iface_data);

#define MCGG_TYPE_LISTENER (mcgg_listener_get_type())
G_DECLARE_FINAL_TYPE(McggListener, mcgg_listener, MCGG, LISTENER, GObject)
G_DEFINE_TYPE_EXTENDED(McggListener,
                       mcgg_listener,
                       G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER,
                                             mcgg_listener_iface_init))

static void on_enter_cb(GumInvocationListener* listener,
                        GumInvocationContext* ic) {
    auto* self = reinterpret_cast<McggListener*>(listener);
    if (!self->on_enter) return;
    // arm64: arg 0..7 = x0..x7. args[0] = `this` untuk instance method.
    void* args[8];
    for (int i = 0; i < 8; i++)
        args[i] = gum_invocation_context_get_nth_argument(ic, i);
    self->on_enter(args);
}

static void on_leave_cb(GumInvocationListener* listener,
                        GumInvocationContext* ic) {
    auto* self = reinterpret_cast<McggListener*>(listener);
    if (!self->on_leave) return;
    void* args[8];
    for (int i = 0; i < 8; i++)
        args[i] = gum_invocation_context_get_nth_argument(ic, i);
    self->on_leave(args, gum_invocation_context_get_return_value(ic));
}

static void mcgg_listener_class_init(McggListenerClass*) {}
static void mcgg_listener_init(McggListener*) {}

static void mcgg_listener_iface_init(gpointer g_iface, gpointer) {
    auto* iface = static_cast<GumInvocationListenerInterface*>(g_iface);
    iface->on_enter = on_enter_cb;
    iface->on_leave = on_leave_cb;
}

static GumInterceptor* g_interceptor = nullptr;

bool Init() {
    if (g_interceptor) return true;
    gum_init_embedded();
    g_interceptor = gum_interceptor_obtain();
    if (!g_interceptor) {
        LOGE("gum_interceptor_obtain gagal");
        return false;
    }
    LOGI("frida-gum siap");
    return true;
}

bool Attach(void* addr, EnterFn on_enter, LeaveFn on_leave, const char* label) {
    if (!addr) { LOGW("hook %s: alamat null, dilewati", label); return false; }
    if (!Init()) return false;

    auto* l = static_cast<McggListener*>(
        g_object_new(MCGG_TYPE_LISTENER, nullptr));
    l->on_enter = on_enter;
    l->on_leave = on_leave;
    l->label    = label;

    gum_interceptor_begin_transaction(g_interceptor);
    GumAttachReturn r = gum_interceptor_attach(
        g_interceptor, addr,
        reinterpret_cast<GumInvocationListener*>(l), nullptr);
    gum_interceptor_end_transaction(g_interceptor);

    if (r != GUM_ATTACH_OK) {
        LOGE("hook %s GAGAL (kode %d) @ %p", label, (int)r, addr);
        return false;
    }
    LOGI("hook OK: %s @ %p", label, addr);
    return true;
}

} // namespace hook
