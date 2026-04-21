#include <gtest/gtest.h>
#include "dtls/dtls_transport.h"
#include <vector>
#include <cstdio>

// 模拟握手：server 和 client 通过内存交换数据
static void exchange(DtlsTransport& sender, DtlsTransport& receiver) {
    auto data = sender.pending_send();
    if (!data.empty()) {
        receiver.feed_data(data.data(), data.size());
    }
}

TEST(DtlsTransport, Handshake) {
    DtlsTransport server(true), client(false);
    ASSERT_TRUE(server.init());
    ASSERT_TRUE(client.init());

    // 最多 20 轮握手交换
    for (int i = 0; i < 20 && (!server.handshake_done() || !client.handshake_done()); ++i) {
        client.drive_handshake();
        exchange(client, server);

        server.drive_handshake();
        exchange(server, client);

        client.drive_timeout();
        server.drive_timeout();
    }

    EXPECT_TRUE(server.handshake_done());
    EXPECT_TRUE(client.handshake_done());
}

TEST(DtlsTransport, KeyMaterial) {
    DtlsTransport server(true), client(false);
    ASSERT_TRUE(server.init());
    ASSERT_TRUE(client.init());

    for (int i = 0; i < 20 && (!server.handshake_done() || !client.handshake_done()); ++i) {
        client.drive_handshake();
        exchange(client, server);
        server.drive_handshake();
        exchange(server, client);
        client.drive_timeout();
        server.drive_timeout();
    }

    ASSERT_TRUE(server.handshake_done());
    ASSERT_TRUE(client.handshake_done());

    uint8_t server_km[60] = {};
    uint8_t client_km[60] = {};
    ASSERT_TRUE(server.export_srtp_keying_material(server_km));
    ASSERT_TRUE(client.export_srtp_keying_material(client_km));

    // 两端导出的密钥材料应相同
    EXPECT_EQ(memcmp(server_km, client_km, 60), 0);

    // 非全零
    bool nonzero = false;
    for (int i = 0; i < 60; ++i) nonzero |= (server_km[i] != 0);
    EXPECT_TRUE(nonzero);
}

TEST(DtlsTransport, IsDetection) {
    // DTLS 记录类型 20=change_cipher_spec, 22=handshake, 23=application_data
    uint8_t dtls_pkt[5] = {22, 0xFE, 0xFF, 0, 0};
    EXPECT_TRUE(DtlsTransport::is_dtls(dtls_pkt, 5));

    uint8_t stun_pkt[5] = {0x00, 0x01, 0, 0, 0};
    EXPECT_FALSE(DtlsTransport::is_dtls(stun_pkt, 5));

    uint8_t rtp_pkt[5] = {0x80, 0x60, 0, 0, 0};
    EXPECT_FALSE(DtlsTransport::is_dtls(rtp_pkt, 5));
}

TEST(DtlsTransport, Fingerprint) {
    DtlsTransport transport(true);
    ASSERT_TRUE(transport.init());
    std::string fp = transport.local_fingerprint();
    EXPECT_FALSE(fp.empty());
    // 格式: XX:XX:...（至少有一个冒号）
    EXPECT_NE(fp.find(':'), std::string::npos);
}
