# cmake/toolchain-linux.cmake
# Toolchain para compilar AINE en Linux (x86_64 o aarch64)
# Uso: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-linux.cmake

set(CMAKE_SYSTEM_NAME Linux)

# Auto-detectar arquitectura o usar la del host
if(NOT CMAKE_SYSTEM_PROCESSOR)
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE CMAKE_SYSTEM_PROCESSOR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

# Preferir clang si está disponible, fallback a gcc
find_program(CMAKE_C_COMPILER   clang   clang-15 clang-14 gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER clang++ clang++-15 clang++-14 g++ REQUIRED)

# Flags estándar Linux
set(CMAKE_C_FLAGS_INIT   "-fPIC")
set(CMAKE_CXX_FLAGS_INIT "-fPIC")

# En aarch64: flags específicas para compatibilidad con Android
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(CMAKE_C_FLAGS_INIT   "${CMAKE_C_FLAGS_INIT}   -march=armv8-a")
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -march=armv8-a")
    message(STATUS "Linux aarch64: native ARM64 execution for Android apps")
endif()

# ccache si está disponible
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Linker: preferir lld (más rápido)
find_program(LLD_LINKER ld.lld)
if(LLD_LINKER)
    set(CMAKE_EXE_LINKER_FLAGS_INIT    "-fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
endif()
