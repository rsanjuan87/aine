# src/aine-hals/input/CMakeLists.txt — aine-input (keyboard + mouse → Android events)

# Pure C / keymap + queue (no AppKit dependency, fully testable headlessly)
add_library(aine-input-core STATIC
    input_keymap.c
    inputflinger.c
)
target_include_directories(aine-input-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(aine-input-core PRIVATE -Wall -Wextra -Wno-unused-parameter)

# NSEvent bridge (Objective-C++, requires AppKit — macOS only)
if(AINE_PLATFORM_MACOS)
    add_library(aine-input SHARED
        keyboard.mm
        pointer.mm
    )
    set_target_properties(aine-input PROPERTIES
        OUTPUT_NAME "aine-input"
        SOVERSION 1
    )
    target_include_directories(aine-input PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(aine-input PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -fvisibility=default
        -fobjc-arc
    )
    target_link_libraries(aine-input PUBLIC
        aine-input-core
        "-framework AppKit"
        "-framework Foundation"
    )
endif()
