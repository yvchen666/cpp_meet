#include <gtest/gtest.h>
#include "rtp/rtcp_packet.h"
#include <algorithm>

TEST(RtcpSR, SerializeAndParse) {
    RtcpSR sr;
    sr.ssrc          = 0xAABBCCDD;
    sr.ntp_sec       = 3900000000u;
    sr.ntp_frac      = 123456789u;
    sr.rtp_timestamp = 90000u;
    sr.packet_count  = 500u;
    sr.octet_count   = 128000u;

    auto bytes = sr.serialize();
    ASSERT_EQ(bytes.size(), 28u);

    // 验证 common header
    EXPECT_EQ(bytes[0], 0x80);   // V=2, P=0, RC=0
    EXPECT_EQ(bytes[1], 200);    // PT=SR

    RtcpSR sr2;
    ASSERT_TRUE(sr2.parse(bytes.data(), bytes.size()));

    EXPECT_EQ(sr2.ssrc,          sr.ssrc);
    EXPECT_EQ(sr2.ntp_sec,       sr.ntp_sec);
    EXPECT_EQ(sr2.ntp_frac,      sr.ntp_frac);
    EXPECT_EQ(sr2.rtp_timestamp, sr.rtp_timestamp);
    EXPECT_EQ(sr2.packet_count,  sr.packet_count);
    EXPECT_EQ(sr2.octet_count,   sr.octet_count);
}

TEST(RtcpNack, SerializeAndParse) {
    RtcpNack nack;
    nack.sender_ssrc = 0x11223344;
    nack.media_ssrc  = 0x55667788;
    // 多个连续和非连续序列号
    nack.lost_seqs   = {100, 101, 102, 105, 110, 200};

    auto bytes = nack.serialize();
    ASSERT_FALSE(bytes.empty());

    // 验证 common header
    EXPECT_EQ(bytes[0], 0x81);  // V=2, P=0, FMT=1
    EXPECT_EQ(bytes[1], 205);   // PT=205

    RtcpNack nack2;
    ASSERT_TRUE(nack2.parse(bytes.data(), bytes.size()));

    EXPECT_EQ(nack2.sender_ssrc, nack.sender_ssrc);
    EXPECT_EQ(nack2.media_ssrc,  nack.media_ssrc);

    // 解包后所有原始序列号必须都出现在 lost_seqs 中
    for (uint16_t seq : nack.lost_seqs) {
        auto it = std::find(nack2.lost_seqs.begin(), nack2.lost_seqs.end(), seq);
        EXPECT_NE(it, nack2.lost_seqs.end()) << "seq " << seq << " missing after parse";
    }
}

TEST(RtcpNack, BLPCompression) {
    // 测试 BLP 压缩效率：17 个连续序列号应该被压缩为 2 个 FSN+BLP 对
    RtcpNack nack;
    nack.sender_ssrc = 0x01;
    nack.media_ssrc  = 0x02;
    // FSN=50, BLP 覆盖 51-66（16个），序列号 67 需要第二个对
    for (uint16_t s = 50; s <= 67; ++s) {
        nack.lost_seqs.push_back(s);
    }

    auto bytes = nack.serialize();
    // 2 个 FSN+BLP 对 => 总长度 = 12 + 2*4 = 20
    EXPECT_EQ(bytes.size(), 20u);

    RtcpNack nack2;
    ASSERT_TRUE(nack2.parse(bytes.data(), bytes.size()));
    EXPECT_EQ(nack2.lost_seqs.size(), 18u);  // 50..67 共 18 个
}
