#include <gtest/gtest.h>
#include "weak/nack_manager.h"
#include <thread>

TEST(NackManager, DetectGap) {
    NackManager mgr;

    auto t0 = std::chrono::steady_clock::now();

    // 收到 1, 2, 4（缺3）
    mgr.on_receive(1);
    mgr.on_receive(2);
    mgr.on_receive(4);

    // 100ms 之前，get_nack_list 应当返回空（未到超时）
    auto nack_early = mgr.get_nack_list(t0 + std::chrono::milliseconds(50));
    EXPECT_TRUE(nack_early.empty());

    // 等待超过 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(110));

    auto nack_list = mgr.get_nack_list(std::chrono::steady_clock::now());
    ASSERT_EQ(nack_list.size(), 1u);
    EXPECT_EQ(nack_list[0], 3u);
}
