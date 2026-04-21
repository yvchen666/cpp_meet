cmake_minimum_required(VERSION 3.20)

add_library(module04_dtls_srtp STATIC
    src/dtls_transport.cpp
    src/srtp_session.cpp
)
target_include_directories(module04_dtls_srtp PUBLIC include)
target_link_libraries(module04_dtls_srtp PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_executable(module04_tests
    tests/test_dtls.cpp
    tests/test_srtp.cpp
)
target_link_libraries(module04_tests PRIVATE module04_dtls_srtp GTest::gtest_main)
add_test(NAME module04_tests COMMAND module04_tests)
