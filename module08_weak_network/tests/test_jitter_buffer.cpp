#include <gtest/gtest.h>
#include "weak/jitter_buffer.h"
#include <thread>

static RtpPacket make_pkt(uint16_t seq, uint32_t ts) {
    RtpPacket p;
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    p.build(96, seq, ts, 0x11223344, payload.data(), payload.size());
    return p;
}

TEST(JitterBuffer, InOrder) {
    JitterBuffer jb(50);

    auto t0 = std::chrono::steady_clock::now();

    // 按序推入 3 个包
    for (int i = 0; i < 3; ++i) {
        jb.push(make_pkt(static_cast<uint16_t>(i), static_cast<uint32_t>(i * 3000)));
    }

    // 等待超过目标延迟
    auto t_pop = t0 + std::chrono::milliseconds(200);
    std::this_thread::sleep_until(t_pop);

    std::vector<uint16_t> out;
    for (int i = 0; i < 5; ++i) {
        auto pkt = jb.pop(std::chrono::steady_clock::now());
        if (pkt) out.push_back(pkt->sequence());
    }

    ASSERT_GE(out.size(), 1u);
    // 输出应按序列号递增顺序
    for (size_t i = 1; i < out.size(); ++i) {
        EXPECT_LT(out[i - 1], out[i]);
    }
}

TEST(JitterBuffer, ReorderAndDelay) {
    JitterBuffer jb(50);

    auto t0 = std::chrono::steady_clock::now();

    // 乱序推入：2, 0, 1
    jb.push(make_pkt(2, 6000));
    jb.push(make_pkt(0, 0));
    jb.push(make_pkt(1, 3000));

    // 等待超过目标延迟
    auto t_pop = t0 + std::chrono::milliseconds(200);
    std::this_thread::sleep_until(t_pop);

    std::vector<uint16_t> out;
    while (true) {
        auto pkt = jb.pop(std::chrono::steady_clock::now());
        if (!pkt) break;
        out.push_back(pkt->sequence());
    }

    ASSERT_EQ(out.size(), 3u);
    // 应当按序列号排序输出（map 保证升序）
    EXPECT_EQ(out[0], 0u);
    EXPECT_EQ(out[1], 1u);
    EXPECT_EQ(out[2], 2u);
}
