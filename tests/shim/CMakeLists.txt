add_executable(test-shim-epoll test_epoll.c)
target_link_libraries(test-shim-epoll PRIVATE aine-shim)
add_test(NAME shim-epoll COMMAND test-shim-epoll)

add_executable(test-shim-futex test_futex.c)
target_link_libraries(test-shim-futex PRIVATE aine-shim)
add_test(NAME shim-futex COMMAND test-shim-futex)

add_executable(test-shim-eventfd test_eventfd.c)
target_link_libraries(test-shim-eventfd PRIVATE aine-shim)
add_test(NAME shim-eventfd COMMAND test-shim-eventfd)

add_executable(test-shim-prctl test_prctl.c)
target_link_libraries(test-shim-prctl PRIVATE aine-shim)
add_test(NAME shim-prctl COMMAND test-shim-prctl)
