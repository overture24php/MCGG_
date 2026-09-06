cmake_minimum_required(VERSION 3.22)
project(mcggmod LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# frida-gum devkit diunduh oleh workflow ke third_party/frida-gum/
set(FRIDA_GUM_DIR "${CMAKE_SOURCE_DIR}/third_party/frida-gum")

add_library(mcggmod SHARED
    src/main.cpp
    src/il2cpp.cpp
    src/hook.cpp
    src/config.cpp
    src/features/autowin.cpp
    src/features/autostack.cpp
    src/features/preclear.cpp
    src/features/freebuy.cpp
)

target_include_directories(mcggmod PRIVATE
    src
    "${FRIDA_GUM_DIR}"
)

# libfrida-gum.a statis: bawa dependensi sendiri, tidak perlu lib eksternal
target_link_libraries(mcggmod
    "${FRIDA_GUM_DIR}/libfrida-gum.a"
    log
    dl
    m
)

# -O2 + strip: kecilkan ukuran, hilangkan simbol yang tidak perlu diekspor
# NDK 29 + libc++: header <locale> (di-include oleh <vector>/<set>) butuh
# _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE, kalau tidak -> "unknown rune table"
target_compile_definitions(mcggmod PRIVATE _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE)

target_compile_options(mcggmod PRIVATE
    -O2
    -fvisibility=hidden
    -fno-rtti
    -Wall
)

target_link_options(mcggmod PRIVATE
    -Wl,--gc-sections
    -Wl,-z,max-page-size=16384
)

set_target_properties(mcggmod PROPERTIES
    OUTPUT_NAME "mcggmod"
    PREFIX "lib"
)
