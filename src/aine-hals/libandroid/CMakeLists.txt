# src/aine-hals/libandroid/CMakeLists.txt — Android libandroid stub dylib

add_library(aine-android SHARED
    native_activity.c
    asset_manager.c
    looper.c
)
set_target_properties(aine-android PROPERTIES
    OUTPUT_NAME "aine-android"
    SOVERSION 1
)
target_compile_options(aine-android PRIVATE -Wall -Wextra -Wno-unused-parameter)
