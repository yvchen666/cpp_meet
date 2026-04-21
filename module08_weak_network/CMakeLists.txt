cmake_minimum_required(VERSION 3.20)
project(module08_weak_network CXX)

add_library(module08_weak_network STATIC
    src/jitter_buffer.cpp
    src/fec.cpp
    src/nack_manager.cpp
    src/gcc_estimator.cpp
)
target_include_directories(module08_weak_network PUBLIC include)
target_link_libraries(module08_weak_network PUBLIC module02_rtp_rtcp)

add_executable(module08_tests
    tests/test_fec.cpp
    tests/test_jitter_buffer.cpp
    tests/test_nack.cpp
)
target_link_libraries(module08_tests PRIVATE module08_weak_network GTest::gtest_main)
add_test(NAME module08_tests COMMAND module08_tests)
