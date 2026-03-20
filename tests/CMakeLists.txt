# Tests — shared (Linux + macOS) + platform-specific

add_subdirectory(shared)
add_subdirectory(shim)
add_subdirectory(binder)
add_subdirectory(pm)
add_subdirectory(loader)
add_subdirectory(dalvik)

if(AINE_PLATFORM_MACOS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/macos")
        add_subdirectory(macos)
    endif()
elseif(AINE_PLATFORM_LINUX)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux")
        add_subdirectory(linux)
    endif()
endif()
