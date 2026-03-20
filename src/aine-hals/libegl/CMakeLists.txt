# src/aine-hals/libegl/CMakeLists.txt — libaine-egl.dylib (Metal-backed EGL 1.4)

set(LIBEGL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(aine-egl SHARED egl.m)
set_target_properties(aine-egl PROPERTIES
    OUTPUT_NAME "aine-egl"
    SOVERSION 1
)
target_include_directories(aine-egl
    PUBLIC  "${LIBEGL_INCLUDE_DIR}"  # exposes EGL/egl.h to consumers
    PRIVATE "${LIBEGL_INCLUDE_DIR}"
)
target_compile_options(aine-egl PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -fobjc-arc
    -fvisibility=default
)
# Metal + QuartzCore (CAMetalLayer) + IOSurface (pbuffer)
target_link_libraries(aine-egl PRIVATE
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Foundation"
    "-framework IOSurface"
    "-framework CoreVideo"
)
