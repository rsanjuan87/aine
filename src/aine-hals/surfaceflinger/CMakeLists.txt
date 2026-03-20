# src/aine-hals/surfaceflinger/CMakeLists.txt — aine-sf (SurfaceFlinger stub)

add_library(aine-sf STATIC sf.c)
target_include_directories(aine-sf
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(aine-sf PRIVATE -Wall -Wextra -Wno-unused-parameter)
