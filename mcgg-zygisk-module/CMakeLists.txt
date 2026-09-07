cmake_minimum_required(VERSION 3.18)
project(mcgg-zygisk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Zygisk module (loader)
add_library(mcgg-zygisk SHARED
    zygisk/main.cpp
)

target_include_directories(mcgg-zygisk PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(mcgg-zygisk
    log
    dl
)

set_target_properties(mcgg-zygisk PROPERTIES
    OUTPUT_NAME "mcgg-zygisk"
    PREFIX "lib"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
