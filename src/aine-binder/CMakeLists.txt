# src/aine-binder/CMakeLists.txt — Binder IPC para macOS via Mach

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

# Crear stubs vacíos para archivos que aún no existen
foreach(src ${BINDER_COMMON_SOURCES} ${BINDER_PLATFORM_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        get_filename_component(d "${CMAKE_CURRENT_SOURCE_DIR}/${src}" DIRECTORY)
        file(MAKE_DIRECTORY "${d}")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/${src}"
            "// AINE stub: ${src}\n// See docs/milestones/M2-binder.md\n")
    endif()
endforeach()

# Biblioteca compartida aine-binder (cliente + protocolo)
add_library(aine-binder SHARED
    ${BINDER_COMMON_SOURCES}
    ${BINDER_PLATFORM_SOURCES}
)
target_include_directories(aine-binder
    PUBLIC  include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/macos
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/linux
)
target_include_directories(aine-binder PRIVATE
    ${CMAKE_SOURCE_DIR}/src/aine-shim/include
)
target_link_libraries(aine-binder PRIVATE ${BINDER_PLATFORM_LIBS})
target_compile_options(aine-binder PRIVATE -fvisibility=hidden)
set_target_properties(aine-binder PROPERTIES
    CXX_STANDARD 17
    C_STANDARD   11
)

# Ejecutable del daemon (proceso independiente)
if(AINE_PLATFORM_MACOS)
    add_executable(aine-binder-daemon
        macos/binder-daemon.cpp
        macos/mach-transport.cpp
        common/parcel.cpp
        common/binder-protocol.cpp
    )
    target_include_directories(aine-binder-daemon
        PRIVATE include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/macos
    )
    target_compile_definitions(aine-binder-daemon PRIVATE AINE_BINDER_MAIN=1)
    set_target_properties(aine-binder-daemon PROPERTIES CXX_STANDARD 17)

    # Herramienta service list (para probar M2)
    add_executable(aine-service
        macos/binder-client.cpp
        macos/binder-daemon.cpp
        macos/mach-transport.cpp
        common/parcel.cpp
        common/binder-protocol.cpp
        common/service-manager.cpp
    )
    target_include_directories(aine-service
        PRIVATE include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/macos
    )
    target_compile_definitions(aine-service PRIVATE AINE_SERVICE_MAIN=1)
    set_target_properties(aine-service PROPERTIES CXX_STANDARD 17)
endif()

