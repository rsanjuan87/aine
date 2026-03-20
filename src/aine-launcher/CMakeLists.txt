# src/aine-launcher — Orquestador de apps Android para macOS
# Coordina instalación y ejecución de APKs via adb

if(AINE_PLATFORM_MACOS)
    add_executable(aine-launcher macos/launcher.cpp)
    target_compile_options(aine-launcher PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
    set_target_properties(aine-launcher PROPERTIES CXX_STANDARD 17)
    install(TARGETS aine-launcher DESTINATION bin)

    # F10: aine-run — AINE-native APK launcher (posix_spawn + pm/apk)
    add_executable(aine-run aine-run.c)
    target_include_directories(aine-run PRIVATE
        ${CMAKE_SOURCE_DIR}/src/aine-pm
    )
    target_link_libraries(aine-run PRIVATE aine-pm-lib)
    target_compile_options(aine-run PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
    install(TARGETS aine-run DESTINATION bin)
endif()
