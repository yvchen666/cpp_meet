add_library(module01_network_core STATIC
    src/udp_socket.cpp
    src/io_context.cpp
)

target_include_directories(module01_network_core PUBLIC include)
target_link_libraries(module01_network_core PUBLIC Threads::Threads)

add_executable(module01_tests
    tests/test_ring_buffer.cpp
    tests/test_udp_socket.cpp
)

target_link_libraries(module01_tests PRIVATE
    module01_network_core
    GTest::gtest_main
)

add_test(NAME module01_tests COMMAND module01_tests)
