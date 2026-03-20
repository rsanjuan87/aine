# src/aine-hals/vulkan — Vulkan HAL (MoltenVK dynamic detection)

add_library(aine-vulkan SHARED vulkan_hal.m)
target_include_directories(aine-vulkan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(aine-vulkan PRIVATE
    -fobjc-arc
    -fvisibility=default
    -Wall -Wextra -Wno-unused-parameter
)
target_link_libraries(aine-vulkan PRIVATE
    "-framework Foundation"
)
