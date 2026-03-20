# src/aine-binder/CMakeLists.txt — platform-aware

set(BINDER_COMMON_SOURCES
    common/parcel.cpp
    common/binder-protocol.cpp
    common/service-manager.cpp
)

if(AINE_PLATFORM_MACOS)
    set(BINDER_PLATFORM_SOURCES
        macos/binder-daemon.cpp
        macos/binder-client.cpp
        macos/mach-transport.cpp
    )
    set(BINDER_PLATFORM_LIBS "")
elseif(AINE_PLATFORM_LINUX)
    set(BINDER_PLATFORM_SOURCES
        linux/binder-daemon.cpp
        linux/binder-client.cpp
        linux/eventfd-compat.cpp
        linux/ashmem-compat.cpp
    )
    set(BINDER_PLATFORM_LIBS pthread)
endif()

foreach(src ${BINDER_COMMON_SOURCES} ${BINDER_PLATFORM_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        get_filename_component(d "${CMAKE_CURRENT_SOURCE_DIR}/${src}" DIRECTORY)
        file(MAKE_DIRECTORY "${d}")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/${src}" "// AINE stub: ${src}\n// See docs/milestones/M2-binder.md\n")
    endif()
endforeach()

add_library(aine-binder SHARED
    ${BINDER_COMMON_SOURCES}
    ${BINDER_PLATFORM_SOURCES}
)
target_include_directories(aine-binder PUBLIC include)
target_link_libraries(aine-binder PRIVATE aine-shim ${BINDER_PLATFORM_LIBS})
