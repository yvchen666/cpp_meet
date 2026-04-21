#include <gtest/gtest.h>
#include "qtclient/pipeline.h"
#include <thread>
#include <chrono>

// 不依赖显示器，仅测试 pipeline 的非 UI 部分
TEST(MediaPipeline, StartStop) {
    MediaPipeline p;
    p.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    p.stop();
    // 不崩溃即通过
    SUCCEED();
}

TEST(MediaPipeline, DoubleStop) {
    MediaPipeline p;
    p.start();
    p.stop();
    p.stop();  // 第二次 stop 不崩溃
    SUCCEED();
}
