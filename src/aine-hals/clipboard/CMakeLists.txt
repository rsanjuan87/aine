# src/aine-hals/clipboard — Clipboard HAL (NSPasteboard ↔ ClipboardManager)

add_library(aine-clipboard SHARED clipboard_hal.m)
target_include_directories(aine-clipboard PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(aine-clipboard PRIVATE
    -fobjc-arc
    -fvisibility=default
    -Wall -Wextra -Wno-unused-parameter
)
target_link_libraries(aine-clipboard PRIVATE
    "-framework AppKit"
    "-framework Foundation"
)
