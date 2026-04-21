add_executable(module12_tests
    tests/test_integration.cpp
)
target_link_libraries(module12_tests PRIVATE
    module09_sfu
    module08_weak_network
    module06_audio
    GTest::gtest_main
)
target_include_directories(module12_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_test(NAME module12_tests COMMAND module12_tests)
