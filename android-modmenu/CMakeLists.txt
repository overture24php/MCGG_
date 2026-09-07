cmake_minimum_required(VERSION 3.18)
project(mcgg_mod_menu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android NDK settings
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 21)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK_HOME})
set(CMAKE_ANDROID_STL_TYPE c++_static)

# Export all symbols for JNI
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/il2cpp
    ${CMAKE_SOURCE_DIR}/src/utils
)

# ImGui source files
set(IMGUI_SOURCES
    include/imgui/imgui.cpp
    include/imgui/imgui_demo.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/backends/imgui_impl_android.cpp
    include/imgui/backends/imgui_impl_opengl3.cpp
)

# Mod menu source files
set(MOD_SOURCES
    src/main.cpp
    src/mod_menu.cpp
    src/mcgg_mod.cpp
    src/il2cpp/il2cpp_api.cpp
    src/utils/memory.cpp
    src/utils/logger.cpp
)

# Create shared library
add_library(mcgg_mod_menu SHARED
    ${IMGUI_SOURCES}
    ${MOD_SOURCES}
)

# Link libraries
target_link_libraries(mcgg_mod_menu
    android
    log
    EGL
    GLESv3
)

# Set output properties
set_target_properties(mcgg_mod_menu PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Install
install(TARGETS mcgg_mod_menu DESTINATION lib/arm64-v8a)
