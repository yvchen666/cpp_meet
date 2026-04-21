add_library(module02_rtp_rtcp STATIC
    src/rtp_packet.cpp
    src/rtcp_packet.cpp
    src/rtp_jitter_buffer.cpp
)
target_include_directories(module02_rtp_rtcp PUBLIC include)
target_link_libraries(module02_rtp_rtcp PUBLIC module01_network_core)

add_executable(module02_tests
    tests/test_rtp_packet.cpp
    tests/test_rtcp_packet.cpp
)
target_link_libraries(module02_tests PRIVATE module02_rtp_rtcp GTest::gtest_main)
add_test(NAME module02_tests COMMAND module02_tests)
