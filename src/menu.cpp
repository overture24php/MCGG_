// ---------------------------------------------------------------------------
// Menu overlay Dear ImGui untuk MCGG Android Mod.
// Pola diambil dari PolarImGui / NightcoreLios / Zygisk-ImGui-Mod-Menu:
//   1. Hook eglSwapBuffers (libEGL.so) via Dobby inline hook
//   2. Di eglSwapBuffers: ImGui_ImplOpenGL3_NewFrame + ImGui_ImplAndroid_NewFrame
//   3. Render menu (checkbox fitur, status, counter)
//   4. ImGui_ImplOpenGL3_RenderDrawData sebelum swap
//   5. Touch input: hook InputConsumer atau nativeInjectEvent
//
// Perbedaan utama: kita pakai Dobby (ringan), bukan gumpp untuk hook GL.
// Lebih stabil untuk game yang sudah punya anti-cheat ringan.
// ---------------------------------------------------------------------------
#include "menu.h"
#include "config.h"
#include "il2cpp.h"
#include "log.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_android.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dobby.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>

namespace menu {

static bool g_initialized = false;
static bool g_menu_visible = true;
static int  g_width = 1080, g_height = 1920;

// Dobby hook eglSwapBuffers
typedef EGLBoolean (*SwapBuffersFn)(EGLDisplay, EGLSurface);
static SwapBuffersFn g_original_swap = nullptr;

// hook touch: InputConsumer::initializeMotionEvent
// (simplifikasi: pakai ImGui_ImplAndroid_HandleInputEvent via hook)

static EGLBoolean HookedSwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (!g_initialized) {
        if (g_original_swap) return g_original_swap(display, surface);
        return EGL_TRUE;
    }

    // pastikan GL context aktif
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) {
        if (g_original_swap) return g_original_swap(display, surface);
        return EGL_TRUE;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(g_width, g_height);
    ImGui::NewFrame();

    // ---- overlay ----
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("MCGG Mod v1.0", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    // status
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status:");
    ImGui::SameLine();
    ImGui::Text("%s", il2::CsImage() ? "READY" : "WAITING");

    ImGui::Separator();

    // 6 fitur
    ImGui::Checkbox("1. Shop Pre-Clear", &cfg::t.preclear);
    ImGui::Checkbox("2. Auto Buy Guinevere", &cfg::t.autobuy_guin);
    ImGui::Checkbox("3. Auto Win Bypass", &cfg::t.autowin_bypass);

    if (ImGui::Button("4. Auto Win (Trigger)", ImVec2(-1, 0))) {
        cfg::t.autowin = true;
    }

    ImGui::Checkbox("5. Auto Stack 14", &cfg::t.autostack);
    if (ImGui::Button("6. Clear Stack", ImVec2(-1, 0))) {
        cfg::t.clear_stack = true;
    }

    ImGui::Separator();
    ImGui::Text("Stack: %d/%d", cfg::stack_count, cfg::stack_target);

    // toggle visibility hint
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Tap outside menu to hide");

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // swap asli
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
    io.DisplaySize = ImVec2((float)g_width, (float)g_height);
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.85f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0, 1, 0, 1);

    // backend
    if (!ImGui_ImplAndroid_Init(nullptr)) {
        LOGE("ImGui_ImplAndroid_Init gagal");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        LOGE("ImGui_ImplOpenGL3_Init gagal");
        return false;
    }

    // hook eglSwapBuffers via Dobby
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

    // Dobby hook
    if (DobbyHook(swap, (void*)HookedSwapBuffers, (void**)&g_original_swap) != 0) {
        LOGE("DobbyHook eglSwapBuffers gagal");
        return false;
    }

    g_initialized = true;
    LOGI("menu ImGui siap (eglSwapBuffers hooked)");
    return true;
}

void Shutdown() {
    if (!g_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
    g_initialized = false;
}

} // namespace menu
