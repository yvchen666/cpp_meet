#include <gtest/gtest.h>
#include <net/udp_socket.h>
#include <rtp/rtp_packet.h>
#include <weak/fec.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

// 集成测试：两个端点通过 loopback UDP 互发 RTP 包
TEST(Integration, TwoEndpointsRtpExchange) {
    net::UdpSocket sender, receiver;
    ASSERT_TRUE(sender.bind(15100));
    ASSERT_TRUE(receiver.bind(15101));

    std::atomic<int> recv_count{0};
    std::atomic<bool> running{true};

    // 接收线程
    std::thread recv_thread([&] {
        uint8_t buf[2048];
        sockaddr_in from{};
        while (running.load()) {
            ssize_t n = receiver.recv_from(buf, sizeof(buf), from);
            if (n > 0) {
                RtpPacket pkt;
                if (pkt.parse(buf, static_cast<size_t>(n))) {
                    recv_count.fetch_add(1);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // 发送线程：150帧 ≈ 5秒
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dst.sin_port = htons(15101);

    uint16_t seq = 0;
    uint32_t ts  = 0;
    for (int i = 0; i < 150; ++i) {
        RtpPacket pkt;
        uint8_t payload[100]{};
        pkt.build(96, seq++, ts, 0xDEADBEEF, payload, sizeof(payload));
        sender.send_to(pkt.data(), pkt.size(), dst);
        ts += 3000;  // 90000Hz / 30fps
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    running = false;
    recv_thread.join();

    EXPECT_GT(recv_count.load(), 100);
    std::cout << "Received " << recv_count.load() << " / 150 RTP packets\n";
}

// 测试 FEC 在每5个包丢1个（20%）情况下的恢复能力
TEST(Integration, FecRecoveryUnder20PercentLoss) {
    XorFecEncoder enc(5);
    XorFecDecoder dec(5);
    int lost = 0, recovered = 0;

    for (int i = 0; i < 100; ++i) {
        RtpPacket pkt;
        uint8_t pl[50]{};
        pl[0] = static_cast<uint8_t>(i);
        pkt.build(96, static_cast<uint16_t>(i), static_cast<uint32_t>(i) * 3000,
                  0x12345678, pl, sizeof(pl));

        auto fec_opt = enc.feed(pkt);

        // 丢弃每第5n+2个包（索引 2, 7, 12, ...）
        if (i % 5 != 2) {
            dec.feed(pkt, false);
        } else {
            lost++;
        }

        if (fec_opt) {
            auto rec = dec.feed(*fec_opt, true);
            if (rec) recovered++;
        }
    }

    EXPECT_EQ(lost, 20);
    EXPECT_GE(recovered, 15);  // 至少恢复 75%
    std::cout << "FEC: lost=" << lost << " recovered=" << recovered << "\n";
}
