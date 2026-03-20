# src/aine-hals/vsync/CMakeLists.txt — aine-vsync (CADisplayLink-backed VSYNC)

add_library(aine-vsync STATIC vsync.m)
target_include_directories(aine-vsync
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(aine-vsync PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -fobjc-arc
)
target_link_libraries(aine-vsync PRIVATE
    "-framework QuartzCore"
    "-framework Foundation"
    "-framework CoreVideo"
)
