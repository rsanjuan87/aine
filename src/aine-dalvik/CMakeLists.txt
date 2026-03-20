# src/aine-dalvik/CMakeLists.txt
# Native Dalvik bytecode interpreter for macOS ARM64 — no Android emulator needed.
# M1 milestone: executes HelloWorld.dex natively on macOS with no adb bridge.

cmake_minimum_required(VERSION 3.20)

set(DALVIK_SOURCES
    dex.c
    heap.c
    jni.c
    interp.c
    main.c
)

add_executable(dalvikvm ${DALVIK_SOURCES})

target_include_directories(dalvikvm PRIVATE .)

# C11 required for designated initialisers and _Atomic (not used yet but anticipated)
set_target_properties(dalvikvm PROPERTIES C_STANDARD 11)

# Strict compilation
target_compile_options(dalvikvm PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# aine-shim is DYLD_INSERT_LIBRARIES at runtime — no link dependency.
# dalvikvm links only against the system libraries.
if(APPLE)
    target_link_libraries(dalvikvm PRIVATE)
endif()

# Install to build output root so run-app.sh can find it with:
#   find "$BUILD_DIR" -name "dalvikvm"
set_target_properties(dalvikvm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
