cmake_minimum_required(VERSION 3.22)
project(aine VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Platform detection
include(cmake/platform.cmake)

option(AINE_ART_WORKAROUND_JIT "Force JIT (Apple Silicon 16KB page workaround)" ON)
option(AINE_ATL_COMPAT "ATL-compatible install paths (Linux)" OFF)
option(AINE_ENABLE_DEBUG_SHIM "Verbose logging in aine-shim" OFF)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -fvisibility=hidden)
if(AINE_ENABLE_DEBUG_SHIM)
    add_compile_definitions(AINE_DEBUG=1)
endif()

if(AINE_PLATFORM_MACOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")
    add_compile_definitions(AINE_PAGE_SIZE=16384 AINE_PLATFORM_MACOS=1)
    include_directories(src/aine-shim/include)
elseif(AINE_PLATFORM_LINUX)
    add_compile_definitions(AINE_PAGE_SIZE=4096 AINE_PLATFORM_LINUX=1)
    if(AINE_ATL_COMPAT)
        set(CMAKE_INSTALL_PREFIX "/usr/lib/android-translation-layer")
    endif()
endif()

include(cmake/atl-integration.cmake)

add_subdirectory(src/aine-shim)
add_subdirectory(src/aine-binder)
add_subdirectory(src/aine-hals)
add_subdirectory(src/aine-launcher)
add_subdirectory(src/aine-dalvik)
add_subdirectory(src/aine-pm)
add_subdirectory(src/aine-loader)
add_subdirectory(test-apps/native-stub)

enable_testing()
add_subdirectory(tests)

message(STATUS "AINE ${PROJECT_VERSION} — platform: ${AINE_PLATFORM}")
