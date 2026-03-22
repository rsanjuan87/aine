# src/aine-dalvik/CMakeLists.txt
# Native Dalvik bytecode interpreter for macOS ARM64 — no Android emulator needed.
# M1 milestone: executes HelloWorld.dex natively on macOS with no adb bridge.

cmake_minimum_required(VERSION 3.20)

set(DALVIK_SOURCES
    dex.c
    heap.c
    jni.c
    layout.c
    handler.c
    interp.c
    main.c
)

# G5: --window mode — NSApplication + NSWindow bootstrap
# G6/T8.2: keyboard.mm + pointer.mm — NSEvent → Android event bridge
# G7/T9.2: canvas.m — CGBitmapContext software canvas for Android Canvas ops
# Only compiled on macOS (Objective-C + AppKit)
if(APPLE)
    list(APPEND DALVIK_SOURCES window.m canvas.m)
    list(APPEND DALVIK_SOURCES
        "${CMAKE_SOURCE_DIR}/src/aine-hals/input/keyboard.mm"
        "${CMAKE_SOURCE_DIR}/src/aine-hals/input/pointer.mm"
    )
endif()

add_executable(dalvikvm ${DALVIK_SOURCES})

target_include_directories(dalvikvm PRIVATE .
    "${CMAKE_SOURCE_DIR}/src/aine-hals/input"
)

# C11 required for designated initialisers and _Atomic (not used yet but anticipated)
set_target_properties(dalvikvm PROPERTIES C_STANDARD 11)

# Strict compilation
target_compile_options(dalvikvm PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# window.m is Objective-C and needs ARC + AppKit frameworks
# keyboard.mm / pointer.mm are Objective-C++ with ARC
if(APPLE)
    set_source_files_properties(window.m PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    set_source_files_properties(canvas.m PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/src/aine-hals/input/keyboard.mm"
        "${CMAKE_SOURCE_DIR}/src/aine-hals/input/pointer.mm"
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(dalvikvm PRIVATE
        aine-input-core
        "-framework Foundation"
        "-framework AppKit"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework CoreGraphics"
        "-framework CoreText"
    )
endif()

# Install to build output root so run-app.sh can find it with:
#   find "$BUILD_DIR" -name "dalvikvm"
set_target_properties(dalvikvm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
