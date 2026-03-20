# src/aine-hals/camera — Camera HAL (AVCaptureSession backend)

add_library(aine-camera SHARED camera_hal.m)
target_include_directories(aine-camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(aine-camera PRIVATE
    -fobjc-arc
    -fvisibility=default
    -Wall -Wextra -Wno-unused-parameter
)
target_link_libraries(aine-camera PRIVATE
    "-framework AVFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Foundation"
)
