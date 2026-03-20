add_executable(test-shim-epoll test_epoll.c)
target_link_libraries(test-shim-epoll PRIVATE aine-shim)
add_test(NAME shim-epoll COMMAND test-shim-epoll)
