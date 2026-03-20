# cmake/platform.cmake
# AINE platform detection — sets AINE_PLATFORM and AINE_PLATFORM_* variables

if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(AINE_PLATFORM "macos-arm64" CACHE STRING "AINE target platform" FORCE)
        set(AINE_PLATFORM_MACOS TRUE CACHE BOOL "" FORCE)
        message(STATUS "[AINE] Platform: macOS ARM64 (Apple Silicon) — primary target")
    else()
        set(AINE_PLATFORM "macos-x86_64" CACHE STRING "AINE target platform" FORCE)
        set(AINE_PLATFORM_MACOS TRUE CACHE BOOL "" FORCE)
        message(WARNING "[AINE] Platform: macOS x86_64 — limited support. Apple Silicon recommended.")
        message(WARNING "[AINE] Android native libs (ARM64 .so) won't run without Rosetta JIT support.")
    endif()

elseif(UNIX AND NOT APPLE)
    # Detect Linux architecture
    execute_process(COMMAND uname -m OUTPUT_VARIABLE _ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(_ARCH STREQUAL "aarch64")
        set(AINE_PLATFORM "linux-aarch64" CACHE STRING "" FORCE)
        message(STATUS "[AINE] Platform: Linux aarch64 — native Android ARM64 execution")
    elseif(_ARCH STREQUAL "x86_64")
        set(AINE_PLATFORM "linux-x86_64" CACHE STRING "" FORCE)
        message(STATUS "[AINE] Platform: Linux x86_64 — ART JIT only (no native ARM64 .so)")
    else()
        set(AINE_PLATFORM "linux-${_ARCH}" CACHE STRING "" FORCE)
        message(WARNING "[AINE] Platform: Linux ${_ARCH} — experimental")
    endif()

    set(AINE_PLATFORM_LINUX TRUE CACHE BOOL "" FORCE)

else()
    message(FATAL_ERROR "[AINE] Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Convenience: are we on a native ARM64 platform (best case for Android apps)?
if(AINE_PLATFORM STREQUAL "macos-arm64" OR AINE_PLATFORM STREQUAL "linux-aarch64")
    set(AINE_NATIVE_ARM64 TRUE CACHE BOOL "Native ARM64 execution possible" FORCE)
else()
    set(AINE_NATIVE_ARM64 FALSE CACHE BOOL "" FORCE)
endif()

message(STATUS "[AINE] Native ARM64 execution: ${AINE_NATIVE_ARM64}")
