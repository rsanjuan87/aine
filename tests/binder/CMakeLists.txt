# tests/binder — Tests de Binder IPC (macOS: Unix socket transport)

if(AINE_PLATFORM_MACOS)
    add_executable(test-binder-roundtrip test_binder_roundtrip.cpp)
    target_link_libraries(test-binder-roundtrip PRIVATE aine-binder)
    target_include_directories(test-binder-roundtrip
        PRIVATE ${CMAKE_SOURCE_DIR}/src/aine-binder/include
    )
    set_target_properties(test-binder-roundtrip PROPERTIES CXX_STANDARD 17)
    add_test(NAME binder-roundtrip COMMAND test-binder-roundtrip)
endif()
