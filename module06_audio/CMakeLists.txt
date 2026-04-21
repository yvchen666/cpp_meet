cmake_minimum_required(VERSION 3.20)
project(module06_audio CXX)

add_library(module06_audio STATIC
    src/opus_codec.cpp
    src/audio_capturer.cpp
)
target_include_directories(module06_audio PUBLIC include)
target_include_directories(module06_audio PRIVATE ${OPUS_INCLUDE_DIRS})
target_link_libraries(module06_audio PUBLIC module02_rtp_rtcp ${OPUS_LIBRARIES})

add_executable(module06_tests tests/test_opus.cpp)
target_link_libraries(module06_tests PRIVATE module06_audio GTest::gtest_main)
add_test(NAME module06_tests COMMAND module06_tests)
