# src/aine-hals/audio/CMakeLists.txt — aine-audio (AudioTrack + AudioFlinger → CoreAudio)

add_library(aine-audio STATIC
    audio_track.c
    audioflinger.c
)
target_include_directories(aine-audio
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(aine-audio PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(aine-audio PRIVATE
    "-framework AudioToolbox"
    "-framework CoreAudio"
    "-framework Foundation"
)
