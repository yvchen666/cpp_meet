cmake_minimum_required(VERSION 3.20)
project(module07_video CXX)

add_library(module07_video STATIC
    src/h264_encoder.cpp
    src/rtp_video_packetizer.cpp
)
target_include_directories(module07_video PUBLIC include)
target_include_directories(module07_video PRIVATE ${X264_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS})
target_link_libraries(module07_video PUBLIC module02_rtp_rtcp
    ${X264_LIBRARIES}
    ${AVCODEC_LIBRARIES} ${AVUTIL_LIBRARIES})

add_executable(module07_tests tests/test_h264.cpp)
target_link_libraries(module07_tests PRIVATE module07_video GTest::gtest_main)
add_test(NAME module07_tests COMMAND module07_tests)
