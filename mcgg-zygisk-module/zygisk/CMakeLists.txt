cmake_minimum_required(VERSION 3.18)
project(mcgg-zygisk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include Zygisk header
target_include_directories(${PROJECT_NAME} PRIVATE
    ${ZYGISK_HEADER_DIR}
)

add_library(${PROJECT_NAME} SHARED
    main.cpp
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
