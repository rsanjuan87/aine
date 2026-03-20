# cmake/toolchain-macos.cmake
# Toolchain para compilar AINE en macOS ARM64 (Apple Silicon)
# Uso: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos.cmake

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# Usar clang de Apple (Xcode CLT)
find_program(CMAKE_C_COMPILER   clang  REQUIRED)
find_program(CMAKE_CXX_COMPILER clang++ REQUIRED)

# Target explícito arm64-apple-macos13
set(CMAKE_C_COMPILER_TARGET   "arm64-apple-macos13.0")
set(CMAKE_CXX_COMPILER_TARGET "arm64-apple-macos13.0")
set(CMAKE_OBJC_COMPILER_TARGET "arm64-apple-macos13.0")

# Flags de compilación para Apple Silicon
set(CMAKE_C_FLAGS_INIT   "-arch arm64 -mmacosx-version-min=13.0")
set(CMAKE_CXX_FLAGS_INIT "-arch arm64 -mmacosx-version-min=13.0")

# sysroot del SDK de macOS
execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE MACOS_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_OSX_SYSROOT "${MACOS_SDK_PATH}")
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

# No hacer búsqueda de programas en el host durante cross-compile
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# ccache si está disponible
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache: ${CCACHE_PROGRAM}")
endif()
