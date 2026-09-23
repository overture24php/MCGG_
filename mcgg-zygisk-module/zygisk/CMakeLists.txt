# Zygisk loader MCGG — build dari folder ini (cmake -S . -B build).
# zygisk.hpp BUNDLED di folder ini (tidak download saat build).
cmake_minimum_required(VERSION 3.18)
project(mcgg-zygisk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED
    main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZYGISK_HEADER_DIR}
)

target_link_libraries(${PROJECT_NAME}
    log
    dl
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "mcgg-zygisk"
    PREFIX "lib"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)