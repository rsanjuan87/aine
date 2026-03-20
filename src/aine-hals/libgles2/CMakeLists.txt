# src/aine-hals/libgles2/CMakeLists.txt — libaine-gles2.dylib (GLES2 stubs)

set(LIBGLES2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(aine-gles2 SHARED gles2.c)
set_target_properties(aine-gles2 PROPERTIES
    OUTPUT_NAME "aine-gles2"
    SOVERSION 1
)
target_include_directories(aine-gles2
    PUBLIC  "${LIBGLES2_INCLUDE_DIR}"   # exposes GLES2/gl2.h to consumers
    PRIVATE "${LIBGLES2_INCLUDE_DIR}"
)
target_compile_options(aine-gles2 PRIVATE -Wall -Wextra -Wno-unused-parameter -fvisibility=default)
# Pure C, no framework dependencies needed for stub functions
