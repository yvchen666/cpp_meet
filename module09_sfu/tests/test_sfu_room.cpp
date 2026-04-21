#include <gtest/gtest.h>
#include "sfu/sfu_room.h"
#include <cstring>
#include <vector>

// 构造一个最小有效 RTP 包（12字节头 + payload）
static std::vector<uint8_t> make_rtp(uint32_t ssrc, uint8_t payload_byte) {
    std::vector<uint8_t> pkt(13, 0);
    pkt[0] = 0x80;  // V=2
    pkt[1] = 96;    // PT=96
    // SSRC at bytes 8-11
    pkt[8]  = (ssrc >> 24) & 0xFF;
    pkt[9]  = (ssrc >> 16) & 0xFF;
    pkt[10] = (ssrc >>  8) & 0xFF;
    pkt[11] =  ssrc        & 0xFF;
    pkt[12] = payload_byte;
    return pkt;
}

TEST(SfuRoom, RoutingToOtherPeers) {
    struct Recv {
        int peer_id;
        std::vector<uint8_t> data;
    };
    std::vector<Recv> received;

    SfuRoom room("test_room", [&](int peer_id, const uint8_t* data, size_t len) {
        received.push_back({peer_id, std::vector<uint8_t>(data, data + len)});
    });

    room.add_peer(0);
    room.add_peer(1);
    room.add_peer(2);

    auto pkt = make_rtp(0xAABBCCDD, 0x42);
    room.route_rtp(0, pkt.data(), pkt.size());

    // peer0 が送信者なので受信しない、peer1 と peer2 だけ受信
    ASSERT_EQ(received.size(), 2u);
    bool got1 = false, got2 = false;
    for (auto& r : received) {
        if (r.peer_id == 1) got1 = true;
        if (r.peer_id == 2) got2 = true;
        EXPECT_NE(r.peer_id, 0);
    }
    EXPECT_TRUE(got1);
    EXPECT_TRUE(got2);
}

TEST(SfuRoom, SsrcRewrite) {
    std::vector<std::vector<uint8_t>> received_pkts;

    SfuRoom room("ssrc_room", [&](int /*peer_id*/, const uint8_t* data, size_t len) {
        received_pkts.push_back(std::vector<uint8_t>(data, data + len));
    });

    room.add_peer(10);
    room.add_peer(20);

    // 原始 SSRC
    uint32_t original_ssrc = 0xDEADBEEF;
    auto pkt = make_rtp(original_ssrc, 0x01);
    room.route_rtp(10, pkt.data(), pkt.size());

    ASSERT_EQ(received_pkts.size(), 1u);
    auto& rpkt = received_pkts[0];
    ASSERT_GE(rpkt.size(), 12u);

    // 读出重写后的 SSRC
    uint32_t rewritten_ssrc =
        ((uint32_t)rpkt[8]  << 24) |
        ((uint32_t)rpkt[9]  << 16) |
        ((uint32_t)rpkt[10] <<  8) |
         (uint32_t)rpkt[11];

    // 重写后 SSRC 应与原始不同（除非刚好相同）
    // 更重要的是：它应该等于 peer10 分配的 out_ssrc（0x10000000）
    EXPECT_EQ(rewritten_ssrc, 0x10000000u);
    EXPECT_NE(rewritten_ssrc, original_ssrc);
}

TEST(SfuRoom, RemovePeer) {
    std::vector<int> recv_peers;
    SfuRoom room("rm_room", [&](int peer_id, const uint8_t*, size_t) {
        recv_peers.push_back(peer_id);
    });

    room.add_peer(1);
    room.add_peer(2);
    room.add_peer(3);
    room.remove_peer(2);

    auto pkt = make_rtp(0x1111, 0x00);
    room.route_rtp(1, pkt.data(), pkt.size());

    ASSERT_EQ(recv_peers.size(), 1u);
    EXPECT_EQ(recv_peers[0], 3);
}
