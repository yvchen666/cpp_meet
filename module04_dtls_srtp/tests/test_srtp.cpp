#include <gtest/gtest.h>
#include "dtls/srtp_session.h"
#include <vector>
#include <cstring>

// 构造一个简单 RTP 包（12字节头 + payload）
static std::vector<uint8_t> make_rtp(uint16_t seq, uint32_t ssrc,
                                      const char* payload)
{
    std::vector<uint8_t> pkt(12);
    pkt[0] = 0x80; // V=2
    pkt[1] = 0x60; // PT=96 (dynamic)
    pkt[2] = (seq >> 8) & 0xFF;
    pkt[3] = seq & 0xFF;
    // timestamp = 0
    pkt[4] = pkt[5] = pkt[6] = pkt[7] = 0;
    // SSRC
    pkt[8]  = (ssrc >> 24) & 0xFF;
    pkt[9]  = (ssrc >> 16) & 0xFF;
    pkt[10] = (ssrc >> 8)  & 0xFF;
    pkt[11] = ssrc & 0xFF;
    // payload
    size_t plen = strlen(payload);
    pkt.insert(pkt.end(), payload, payload + plen);
    return pkt;
}

TEST(SrtpSession, ProtectUnprotect) {
    uint8_t key[16]  = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                        0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10};
    uint8_t salt[14] = {0x11,0x12,0x13,0x14,0x15,0x16,0x17,
                        0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e};

    SrtpSession sender, receiver;
    ASSERT_TRUE(sender.init(key, salt, true));
    ASSERT_TRUE(receiver.init(key, salt, false));

    const char* msg = "Hello, SRTP!";
    auto original = make_rtp(1, 0xDEADBEEF, msg);
    auto pkt = original;

    ASSERT_TRUE(sender.protect(pkt));

    // SRTP 包比原始包长 10 字节（认证标签）
    EXPECT_EQ(pkt.size(), original.size() + 10);

    // 载荷应被加密（与原始不同）
    bool payload_changed = false;
    for (size_t i = 12; i < original.size(); ++i) {
        if (pkt[i] != original[i]) { payload_changed = true; break; }
    }
    EXPECT_TRUE(payload_changed);

    ASSERT_TRUE(receiver.unprotect(pkt));

    // 解密后应与原始相同
    EXPECT_EQ(pkt, original);
}

TEST(SrtpSession, IntegrityCheck) {
    uint8_t key[16]  = {};
    uint8_t salt[14] = {};

    SrtpSession sender, receiver;
    ASSERT_TRUE(sender.init(key, salt, true));
    ASSERT_TRUE(receiver.init(key, salt, false));

    auto pkt = make_rtp(1, 0x12345678, "test payload");
    ASSERT_TRUE(sender.protect(pkt));

    // 篡改一个字节
    pkt[15] ^= 0xFF;

    // 验证应失败
    EXPECT_FALSE(receiver.unprotect(pkt));
}

TEST(SrtpSession, MultiplePackets) {
    uint8_t key[16]  = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00,0x11,
                        0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99};
    uint8_t salt[14] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e};

    SrtpSession sender, receiver;
    ASSERT_TRUE(sender.init(key, salt, true));
    ASSERT_TRUE(receiver.init(key, salt, false));

    for (int seq = 1; seq <= 5; ++seq) {
        auto original = make_rtp(static_cast<uint16_t>(seq), 0xCAFEBABE, "audio_data");
        auto pkt = original;
        ASSERT_TRUE(sender.protect(pkt));
        ASSERT_TRUE(receiver.unprotect(pkt));
        EXPECT_EQ(pkt, original) << "Failed at seq=" << seq;
    }
}
