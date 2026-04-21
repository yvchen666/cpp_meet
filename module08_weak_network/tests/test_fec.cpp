#include <gtest/gtest.h>
#include "weak/fec.h"

TEST(XorFec, RecoverOneLoss) {
    XorFecEncoder encoder(5);
    XorFecDecoder decoder(5);

    // 构建 5 个数据包（payload 各不相同）
    std::vector<RtpPacket> data_pkts;
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> payload(20, static_cast<uint8_t>(i + 1));
        RtpPacket pkt;
        pkt.build(96, static_cast<uint16_t>(i), static_cast<uint32_t>(i * 3000),
                  0xABCDEF01, payload.data(), payload.size());
        data_pkts.push_back(pkt);
    }

    // 编码
    std::optional<RtpPacket> fec_pkt;
    for (int i = 0; i < 5; ++i) {
        fec_pkt = encoder.feed(data_pkts[i]);
    }
    ASSERT_TRUE(fec_pkt.has_value()) << "FEC packet should be generated after group_size packets";

    // 模拟：喂入除第3个包（index=2）之外的所有数据包
    for (int i = 0; i < 5; ++i) {
        if (i == 2) continue;  // 丢失
        decoder.feed(data_pkts[i], false);
    }

    // 喂入 FEC 包，期望恢复第3个包
    auto recovered = decoder.feed(fec_pkt.value(), true);
    ASSERT_TRUE(recovered.has_value()) << "Should recover the lost packet";

    // 验证恢复的 payload 与原始相同
    ASSERT_EQ(recovered->payload_size(), data_pkts[2].payload_size());
    EXPECT_EQ(std::memcmp(recovered->payload(), data_pkts[2].payload(),
                          data_pkts[2].payload_size()), 0)
        << "Recovered payload should match original";
}
