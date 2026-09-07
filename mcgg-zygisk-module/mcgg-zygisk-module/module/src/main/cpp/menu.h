#ifndef MCGG_MENU_H
#define MCGG_MENU_H

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "functions.h"

extern int glWidth, glHeight;
extern bool setupimg;

void SetupImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);
    io.IniFilename = nullptr;
    
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.85f);
    style.ScaleAllSizes(3.0f);
    
    ImGui_ImplOpenGL3_Init("#version 100");
}

void DrawMenu() {
    static bool show_menu = true;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 450), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("MCGG Mod Menu v1.0", &show_menu, 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("Status: %s", IL2CPP::FindClass("", "MCLogicHeroShop") ? "READY" : "WAITING");
    ImGui::Separator();
    
    // 6 fitur
    ImGui::Checkbox("1. Shop Pre-Clear", &g_toggles.preclear);
    ImGui::Checkbox("2. Auto Buy Guinevere", &g_toggles.autobuy_guin);
    ImGui::Checkbox("3. Auto Win Bypass", &g_toggles.autowin_bypass);
    
    if (ImGui::Button("4. Auto Win (Trigger)", ImVec2(-1, 0))) {
        g_toggles.autowin = true;
    }
    
    ImGui::Checkbox("5. Auto Stack 14", &g_toggles.autostack);
    
    if (ImGui::Button("6. Clear Stack", ImVec2(-1, 0))) {
        g_toggles.clear_stack = true;
    }
    
    ImGui::Separator();
    ImGui::Text("Stack: %d/%d", g_stack_count, g_stack_target);
    
    ImGui::End();
}

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);
    
    if (!setupimg) {
        SetupImgui();
        setupimg = true;
    }
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    
    DrawMenu();
    
    ImGui::Render();
    glViewport(0, 0, glWidth, glHeight);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    return old_eglSwapBuffers(dpy, surface);
}

#endif
