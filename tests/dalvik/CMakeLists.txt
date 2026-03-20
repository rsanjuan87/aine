# tests/dalvik/CMakeLists.txt — CTest for F6 aine-dalvik opcode extensions

add_test(NAME dalvik-f6-opcodes
    COMMAND ${CMAKE_BINARY_DIR}/dalvikvm
            -cp ${CMAKE_SOURCE_DIR}/test-apps/DalvikTest/classes.dex Main
)
set_tests_properties(dalvik-f6-opcodes PROPERTIES
    TIMEOUT 15
    LABELS "dalvik;f6"
    PASS_REGULAR_EXPRESSION "RESULT: 8 ok, 0 failed"
)
