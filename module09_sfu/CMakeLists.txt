add_library(module09_sfu STATIC
    src/sfu_room.cpp
    src/sfu_server.cpp
)
target_include_directories(module09_sfu PUBLIC include)
target_link_libraries(module09_sfu PUBLIC
    module01_network_core
    module02_rtp_rtcp
)

add_executable(module09_tests tests/test_sfu_room.cpp)
target_link_libraries(module09_tests PRIVATE module09_sfu GTest::gtest_main)
add_test(NAME module09_tests COMMAND module09_tests)
