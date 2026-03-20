# src/aine-launcher — Orquestador de apps Android para macOS
# Coordina instalación y ejecución de APKs via adb

if(AINE_PLATFORM_MACOS)
    add_executable(aine-launcher macos/launcher.cpp)
    target_compile_options(aine-launcher PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
    set_target_properties(aine-launcher PROPERTIES CXX_STANDARD 17)
    install(TARGETS aine-launcher DESTINATION bin)
endif()
