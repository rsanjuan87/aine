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

