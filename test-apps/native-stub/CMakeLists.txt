# test-apps/native-stub/CMakeLists.txt — ARM64 native stub dylib for loader tests

add_library(aine-native-stub SHARED native_stub.c)
set_target_properties(aine-native-stub PROPERTIES
    OUTPUT_NAME "aine-native-stub"
)
target_compile_options(aine-native-stub PRIVATE -Wall -Wno-unused-parameter)
