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

