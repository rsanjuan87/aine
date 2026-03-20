# src/aine-hals/liblog/CMakeLists.txt — Android liblog stub dylib

add_library(aine-log SHARED log.c)
set_target_properties(aine-log PROPERTIES
    OUTPUT_NAME "aine-log"
    SOVERSION 1
)
target_compile_options(aine-log PRIVATE -Wall -Wextra -Wno-unused-parameter)
# No external dependencies — all symbols in libSystem
