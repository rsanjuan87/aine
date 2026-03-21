# tests/macos/CMakeLists.txt — macOS-specific tests

# F7: EGL headless test (Metal + IOSurface, no display required)
add_executable(test-egl-headless test_egl_headless.m)
target_include_directories(test-egl-headless PRIVATE
    "${CMAKE_SOURCE_DIR}/src/aine-hals/libegl/include"
    "${CMAKE_SOURCE_DIR}/src/aine-hals/surface/include"
)
target_compile_options(test-egl-headless PRIVATE -fobjc-arc)
target_link_libraries(test-egl-headless PRIVATE
    aine-egl
    aine-surface
    "-framework Foundation"
)
add_dependencies(test-egl-headless aine-egl aine-surface)

add_test(NAME surface-egl-headless COMMAND test-egl-headless)
set_tests_properties(surface-egl-headless PROPERTIES
    TIMEOUT 15
    LABELS  "graphics;egl;f7;macos"
    PASS_REGULAR_EXPRESSION "RESULT: [0-9]+ ok, 0 failed"
)

# F8: Input HAL test (headless — keymap + queue, no NSEvent needed)
add_executable(test-input test_input.c)
target_link_libraries(test-input PRIVATE aine-input-core)
add_dependencies(test-input aine-input-core)

add_test(NAME input-hal COMMAND test-input)
set_tests_properties(input-hal PROPERTIES
    TIMEOUT 10
    LABELS  "input;f8;macos"
    PASS_REGULAR_EXPRESSION "RESULT: [0-9]+ ok, 0 failed"
)

# F9: Audio HAL test (headless — GenericOutput AudioUnit, no speaker needed)
add_executable(test-audio test_audio.c)
target_link_libraries(test-audio PRIVATE
    aine-audio
    "-framework AudioToolbox"
    "-framework CoreAudio"
    "-framework Foundation"
    m   # math (sin())
)
add_dependencies(test-audio aine-audio)

add_test(NAME audio-hal COMMAND test-audio)
set_tests_properties(audio-hal PROPERTIES
    TIMEOUT 15
    LABELS  "audio;f9;macos"
    PASS_REGULAR_EXPRESSION "RESULT: [0-9]+ ok, 0 failed"
)

# F10: Launcher test — aine-run --list exits 0 (even with empty registry)
add_test(NAME launcher-run COMMAND aine-run --list)
set_tests_properties(launcher-run PROPERTIES
    TIMEOUT 10
    LABELS  "launcher;f10;macos"
)

# F11: Activity lifecycle smoke test — aine-run launches M3TestApp via dalvikvm
# AINE_DALVIKVM points to the built dalvikvm binary so posix_spawn finds it.
add_test(NAME activity-lifecycle
    COMMAND aine-run "${CMAKE_SOURCE_DIR}/test-apps/M3TestApp/M3TestApp.apk"
)
set_tests_properties(activity-lifecycle PROPERTIES
    TIMEOUT 30
    LABELS  "activity;f11;macos"
    ENVIRONMENT "AINE_DALVIKVM=${CMAKE_BINARY_DIR}/dalvikvm"
    PASS_REGULAR_EXPRESSION "onDestroy"
)

# F12: HAL integration tests (Vulkan/MoltenVK detect + Camera + Clipboard)
add_executable(test-f12-hals test_f12_hals.c)
target_include_directories(test-f12-hals PRIVATE
    "${CMAKE_SOURCE_DIR}/src/aine-hals/vulkan"
    "${CMAKE_SOURCE_DIR}/src/aine-hals/camera"
    "${CMAKE_SOURCE_DIR}/src/aine-hals/clipboard"
)
target_link_libraries(test-f12-hals PRIVATE
    aine-vulkan
    aine-camera
    aine-clipboard
)
add_dependencies(test-f12-hals aine-vulkan aine-camera aine-clipboard)

add_test(NAME hals-f12 COMMAND test-f12-hals)
set_tests_properties(hals-f12 PROPERTIES
    TIMEOUT 15
    LABELS  "vulkan;camera;clipboard;f12;macos"
    PASS_REGULAR_EXPRESSION "RESULT: [0-9]+ ok, 0 failed"
)

# G1: Handler.postDelayed fires inside Activity lifecycle (handler-loop)
add_test(NAME handler-loop
    COMMAND dalvikvm -cp "${CMAKE_SOURCE_DIR}/test-apps/HandlerTest/classes.dex" HandlerTest
)
set_tests_properties(handler-loop PROPERTIES
    TIMEOUT 10
    LABELS  "handler;g1;macos"
    PASS_REGULAR_EXPRESSION "handler-fired"
)

# G2: stdlib — ArrayList, HashMap, Math, System.currentTimeMillis, String.format
add_test(NAME g2-stdlib
    COMMAND dalvikvm -cp "${CMAKE_SOURCE_DIR}/test-apps/G2Test/classes.dex" G2Test
)
set_tests_properties(g2-stdlib PROPERTIES
    TIMEOUT 10
    LABELS  "stdlib;g2;macos"
    PASS_REGULAR_EXPRESSION "g2-done"
)

# G3: framework — try/catch, Thread.sleep, Iterator, String.split/replace, Collections.sort
add_test(NAME g3-framework
    COMMAND dalvikvm -cp "${CMAKE_SOURCE_DIR}/test-apps/G3Test/classes.dex" G3Test
)
set_tests_properties(g3-framework PROPERTIES
    TIMEOUT 15
    LABELS  "framework;g3;macos"
    PASS_REGULAR_EXPRESSION "exc-caught.*sleep-ok.*iter:foo,bar,baz,.*split-len:4.*split-2:c.*replace:hello_world.*sort-0:apple.*g3-done"
)

# G4: advanced stdlib — Arrays.asList, Collections.reverse, String.replaceAll, Integer.MAX_VALUE
add_test(NAME g4-stdlib2
    COMMAND dalvikvm -cp "${CMAKE_SOURCE_DIR}/test-apps/G4Test/classes.dex" G4Test
)
set_tests_properties(g4-stdlib2 PROPERTIES
    TIMEOUT 10
    LABELS  "stdlib;g4;macos"
    PASS_REGULAR_EXPRESSION "asList-size:3.*sorted:apple.*reversed:cherry.*replaceAll:hello-world-test.*max-val:2147483647.*g4-done"
)

# G5: NSWindow + Activity display — --window flag creates NSWindow (or skips gracefully
#     on headless), runs full Activity lifecycle, closes window on completion.
add_test(NAME g5-window-activity
    COMMAND dalvikvm --window
            -cp "${CMAKE_SOURCE_DIR}/test-apps/G5WindowTest/classes.dex"
            G5WindowActivity
)
set_tests_properties(g5-window-activity PROPERTIES
    TIMEOUT 30
    LABELS  "window;g5;macos"
    PASS_REGULAR_EXPRESSION "g5-window: onCreate.*g5-window: onResume.*g5-window: onDestroy.*g5-window: done"
)

# G6: View/widget/graphics stubs — Activity with TextView, Button, Toast, Color, LinearLayout
add_test(NAME g6-app-stubs
    COMMAND dalvikvm --window
            -cp "${CMAKE_SOURCE_DIR}/test-apps/G6RealActivity/classes.dex"
            G6RealActivity
)
set_tests_properties(g6-app-stubs PROPERTIES
    TIMEOUT 20
    LABELS  "stubs;g6;macos"
    PASS_REGULAR_EXPRESSION "g6-textview:Hello AINE.*g6-toast-shown.*g6-color:ok.*g6-layout:ok.*g6-onDestroy.*g6-done"
)

# G7: Canvas rendering — drawColor/drawText/drawRect/drawCircle via CoreGraphics
add_test(NAME g7-canvas-draw
    COMMAND dalvikvm --window
            -cp "${CMAKE_SOURCE_DIR}/test-apps/G7DrawApp/classes.dex"
            G7DrawActivity
)
set_tests_properties(g7-canvas-draw PROPERTIES
    TIMEOUT 25
    LABELS  "canvas;g7;macos"
    PASS_REGULAR_EXPRESSION "\\[G7\\] frame:5.*\\[G7\\] draw-complete"
)

# T9.3: AOSP Arcs — first real FOSS app (unmodified AOSP sample, Apache 2.0)
# Runs the Arcs animation for 10 seconds through the full Dalvik→JNI→CoreGraphics stack.
add_test(NAME t93-arcs-aosp
    COMMAND dalvikvm --window
            -cp "${CMAKE_SOURCE_DIR}/test-apps/ArcsApp/classes.dex"
            com.example.android.apis.graphics.Arcs
)
set_tests_properties(t93-arcs-aosp PROPERTIES
    TIMEOUT 20
    LABELS  "foss;t9.3;arcs;macos"
    PASS_REGULAR_EXPRESSION "\\[arcs\\] done"
)

