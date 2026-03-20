# src/aine-hals/surface/CMakeLists.txt — aine-surface (NSWindow + CAMetalLayer)

add_library(aine-surface SHARED surface.m)
set_target_properties(aine-surface PROPERTIES
    OUTPUT_NAME "aine-surface"
    SOVERSION 1
)
target_include_directories(aine-surface
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_options(aine-surface PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -fobjc-arc
    -fvisibility=default
)
target_link_libraries(aine-surface PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Foundation"
    "-framework IOSurface"
    "-framework CoreVideo"
)
