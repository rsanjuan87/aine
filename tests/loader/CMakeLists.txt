# tests/loader/CMakeLists.txt — CTest for F5 aine-loader

add_executable(test-loader test_loader.c)
target_link_libraries(test-loader PRIVATE aine-loader-static)
target_compile_definitions(test-loader PRIVATE
    # Pass build output dirs so the test can find dylibs at runtime
    AINE_BUILD_DIR="$<TARGET_FILE_DIR:aine-log>"
)
# Depend on both stub dylibs so they are built before the test runs
add_dependencies(test-loader aine-log aine-android aine-native-stub)

add_test(NAME loader-path-map COMMAND test-loader)
set_tests_properties(loader-path-map PROPERTIES
    TIMEOUT 10
    LABELS "loader;f5"
)
