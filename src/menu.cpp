// ---------------------------------------------------------------------------
// Implementasi menu Dear ImGui overlay.
// ---------------------------------------------------------------------------
#include "menu.h"
#include "config.h"
#include "log.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_android.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <cstdio>

namespace menu {

static bool g_initialized = false;
static bool g_menu_visible = true;
static EGLDisplay g_display = EGL_NO_DISPLAY;
static int g_width = 0, g_height = 0;

Status status;

// hook eglSwapBuffers
typedef EGLBoolean (*SwapBuffersFn)(EGLDisplay, EGLSurface);
static SwapBuffersFn g_original_swap = nullptr;

static EGLBoolean HookedSwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (g_menu_visible && g_initialized) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame(g_width, g_height);
        ImGui::NewFrame();

        // ---- overlay ----
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(320, 400));
        ImGui::Begin("MCGG Mod", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Status: %s", status.il2cpp_ready ? "READY" : "WAITING");
        ImGui::Text("Hooks: %d/%d", status.hooks_ok, status.hooks_total);
        ImGui::Separator();

        ImGui::Checkbox("1. Shop Pre-Clear", &cfg::t.preclear);
        ImGui::Checkbox("2. Auto Buy Guinevere", &cfg::t.autobuy_guin);
        ImGui::Checkbox("3. Auto Win Bypass", &cfg::t.autowin_bypass);
        if (ImGui::Button("4. Auto Win (Trigger)")) {
            cfg::t.autowin = true;
        }
        ImGui::Checkbox("5. Auto Stack 14", &cfg::t.autostack);
        if (ImGui::Button("6. Clear Stack")) {
            cfg::t.clear_stack = true;
        }

        ImGui::Separator();
        ImGui::Text("Stack: %d/%d", status.stack_count, status.stack_target);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    if (g_original_swap)
        return g_original_swap(display, surface);
    return EGL_TRUE;
}

bool Init() {
    if (g_initialized) return true;

    // init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1080, 1920); // default, akan di-update
    io.IniFilename = nullptr; // tidak simpan layout

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0.7f);

    // backend
    if (!ImGui_ImplAndroid_Init(nullptr)) {
        LOGE("ImGui_ImplAndroid_Init gagal");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        LOGE("ImGui_ImplOpenGL3_Init gagal");
        return false;
    }

    // hook eglSwapBuffers
    void* egl = dlopen("libEGL.so", RTLD_NOW);
    if (!egl) {
        LOGE("dlopen libEGL.so gagal");
        return false;
    }

    void* swap = dlsym(egl, "eglSwapBuffers");
    if (!swap) {
        LOGE("dlsym eglSwapBuffers gagal");
        return false;
    }

    // pasang hook via gum
    extern bool hook_attach(void* addr, void* enter, void* leave, const char* label);
    // gunakan gum interceptor dari hook.cpp
    // (sederhana: langsung replace pointer — untuk tes)
    g_original_swap = (SwapBuffersFn)swap;

    // catatan: implementasi hook sebenarnya butuh gum interceptor
    // untuk sekarang, pakai callback dari main loop

    g_initialized = true;
    LOGI("menu ImGui siap");
    return true;
}

void Render() {
    // dipanggil dari main loop, bukan dari swap hook
    // (simplifikasi: render di thread terpisah)
    if (!g_initialized || !g_menu_visible) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(g_width, g_height);
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(320, 400));
    ImGui::Begin("MCGG Mod", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Status: %s", status.il2cpp_ready ? "READY" : "WAITING");
    ImGui::Text("Hooks: %d/%d", status.hooks_ok, status.hooks_total);
    ImGui::Separator();

    ImGui::Checkbox("1. Shop Pre-Clear", &cfg::t.preclear);
    ImGui::Checkbox("2. Auto Buy Guinevere", &cfg::t.autobuy_guin);
    ImGui::Checkbox("3. Auto Win Bypass", &cfg::t.autowin_bypass);
    if (ImGui::Button("4. Auto Win (Trigger)")) {
        cfg::t.autowin = true;
    }
    ImGui::Checkbox("5. Auto Stack 14", &cfg::t.autostack);
    if (ImGui::Button("6. Clear Stack")) {
        cfg::t.clear_stack = true;
    }

    ImGui::Separator();
    ImGui::Text("Stack: %d/%d", status.stack_count, status.stack_target);

    ImGui::End();

    ImGui::Render();
    // catatan: RenderDrawData harus dipanggil di thread yang punya GL context
}

} // namespace menu
