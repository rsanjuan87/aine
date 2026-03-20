# src/aine-shim/CMakeLists.txt — platform-aware

set(SHIM_COMMON_SOURCES common/logging.c common/aine-shim.c)

if(AINE_PLATFORM_MACOS)
    set(SHIM_PLATFORM_SOURCES
        macos/platform.c
        macos/epoll.c macos/proc.c macos/futex.c macos/ashmem.c
        macos/eventfd.c macos/timerfd.c macos/inotify.c
        macos/signalfd.c macos/prctl.c macos/binder-dev.c)
    set(SHIM_PLATFORM_LIBS "-framework Foundation" "-framework CoreFoundation" "-framework IOKit")
elseif(AINE_PLATFORM_LINUX)
    set(SHIM_PLATFORM_SOURCES linux/passthrough.c linux/binder-dev.c)
    set(SHIM_PLATFORM_LIBS "")
endif()

# Auto-crear stubs para fuentes que aún no existen
foreach(src ${SHIM_PLATFORM_SOURCES} ${SHIM_COMMON_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        get_filename_component(d "${CMAKE_CURRENT_SOURCE_DIR}/${src}" DIRECTORY)
        file(MAKE_DIRECTORY "${d}")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/${src}" "// AINE stub: ${src}\n#include <errno.h>\n")
    endif()
endforeach()

add_library(aine-shim SHARED ${SHIM_COMMON_SOURCES} ${SHIM_PLATFORM_SOURCES})
target_include_directories(aine-shim PUBLIC include)
target_link_libraries(aine-shim PRIVATE ${SHIM_PLATFORM_LIBS})

if(AINE_PLATFORM_LINUX)
    target_compile_definitions(aine-shim PRIVATE AINE_SHIM_PASSTHROUGH=1)
endif()
