cmake_minimum_required(VERSION 3.22)

add_executable(test-pm test_pm.c)
target_link_libraries(test-pm PRIVATE aine-pm-lib)
target_compile_definitions(test-pm PRIVATE
    TEST_APK_PATH="${CMAKE_SOURCE_DIR}/test-apps/M3TestApp/M3TestApp.apk"
    TEST_INSTALL_BASE="/tmp/aine-test"
)

add_test(NAME pm-install COMMAND test-pm)
set_tests_properties(pm-install PROPERTIES TIMEOUT 30)
