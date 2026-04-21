#include <gtest/gtest.h>
#include "rtp/rtp_packet.h"
#include <cstring>

TEST(RtpPacket, BuildAndParse) {
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    const size_t  payload_len = sizeof(payload);

    RtpPacket pkt;
    pkt.build(96, 1234, 90000, 0xDEADBEEF, payload, payload_len, false);

    // 验证原始包字段
    EXPECT_EQ(pkt.version(),      2);
    EXPECT_EQ(pkt.payload_type(), 96);
    EXPECT_EQ(pkt.sequence(),     1234);
    EXPECT_EQ(pkt.timestamp(),    90000u);
    EXPECT_EQ(pkt.ssrc(),         0xDEADBEEFu);
    EXPECT_FALSE(pkt.marker());
    EXPECT_FALSE(pkt.padding());
    EXPECT_FALSE(pkt.extension());
    EXPECT_EQ(pkt.cc(), 0);

    // 重新 parse
    RtpPacket pkt2;
    ASSERT_TRUE(pkt2.parse(pkt.data(), pkt.size()));

    EXPECT_EQ(pkt2.version(),      2);
    EXPECT_EQ(pkt2.payload_type(), 96);
    EXPECT_EQ(pkt2.sequence(),     1234);
    EXPECT_EQ(pkt2.timestamp(),    90000u);
    EXPECT_EQ(pkt2.ssrc(),         0xDEADBEEFu);
}

TEST(RtpPacket, SeqComparison) {
    // 普通比较
    EXPECT_TRUE(RtpPacket::seq_gt(100, 50));
    EXPECT_FALSE(RtpPacket::seq_gt(50, 100));
    EXPECT_FALSE(RtpPacket::seq_gt(100, 100));

    // wrap-around：65535 -> 0 -> 1
    EXPECT_TRUE(RtpPacket::seq_gt(0,     65535));
    EXPECT_TRUE(RtpPacket::seq_gt(1,     65535));
    EXPECT_TRUE(RtpPacket::seq_gt(100,   65500));
    EXPECT_FALSE(RtpPacket::seq_gt(65535, 0));
    EXPECT_FALSE(RtpPacket::seq_gt(65500, 100));
}

TEST(RtpPacket, PayloadIntegrity) {
    const uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    RtpPacket pkt;
    pkt.build(111, 999, 48000, 0x12345678, data, sizeof(data));

    ASSERT_EQ(pkt.payload_size(), sizeof(data));
    EXPECT_EQ(std::memcmp(pkt.payload(), data, sizeof(data)), 0);

    // parse 后 payload 仍然一致
    RtpPacket pkt2;
    ASSERT_TRUE(pkt2.parse(pkt.data(), pkt.size()));
    ASSERT_EQ(pkt2.payload_size(), sizeof(data));
    EXPECT_EQ(std::memcmp(pkt2.payload(), data, sizeof(data)), 0);
}

TEST(RtpPacket, MarkerBit) {
    RtpPacket pkt;
    const uint8_t dummy[] = {0x00};
    pkt.build(96, 1, 0, 0x01, dummy, 1, true);
    EXPECT_TRUE(pkt.marker());

    RtpPacket pkt2;
    pkt2.build(96, 2, 0, 0x01, dummy, 1, false);
    EXPECT_FALSE(pkt2.marker());

    // parse 后 marker bit 保留
    RtpPacket pkt3;
    ASSERT_TRUE(pkt3.parse(pkt.data(), pkt.size()));
    EXPECT_TRUE(pkt3.marker());
}
