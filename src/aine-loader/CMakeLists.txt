# src/aine-loader/CMakeLists.txt — Android .so path mapper + dlopen wrapper

add_library(aine-loader-static STATIC
    path_map.c
    loader.c
)
target_include_directories(aine-loader-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(aine-loader-static PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(aine-loader-test main.c)
target_link_libraries(aine-loader-test PRIVATE aine-loader-static)
