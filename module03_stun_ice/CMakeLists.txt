cmake_minimum_required(VERSION 3.20)

add_library(module03_stun_ice STATIC
    src/stun_message.cpp
    src/ice_agent.cpp
)
target_include_directories(module03_stun_ice PUBLIC include)
target_link_libraries(module03_stun_ice PUBLIC OpenSSL::Crypto)

add_executable(module03_tests tests/test_stun.cpp)
target_link_libraries(module03_tests PRIVATE module03_stun_ice GTest::gtest_main)
add_test(NAME module03_tests COMMAND module03_tests)
