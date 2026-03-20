add_executable(test-binder-protocol test_binder_protocol.cpp)
target_link_libraries(test-binder-protocol PRIVATE aine-binder)
add_test(NAME binder-protocol COMMAND test-binder-protocol)
