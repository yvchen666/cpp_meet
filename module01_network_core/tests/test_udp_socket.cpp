#include <gtest/gtest.h>
#include "net/udp_socket.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <chrono>

using namespace net;

TEST(UdpSocket, BindAndSendRecv) {
    // Receiver binds on port 19876
    UdpSocket receiver;
    ASSERT_TRUE(receiver.bind(19876));

    // Sender: any ephemeral port (no explicit bind needed for sendto)
    UdpSocket sender;

    // Build destination address pointing to receiver
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(19876);
    inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

    const char msg[] = "hello";
    ssize_t sent = sender.send_to(reinterpret_cast<const uint8_t*>(msg),
                                   sizeof(msg), dest);
    ASSERT_EQ(sent, static_cast<ssize_t>(sizeof(msg)));

    // Small spin-wait for the datagram to arrive (non-blocking socket)
    uint8_t buf[64] = {};
    sockaddr_in from{};
    ssize_t received = -1;

    for (int i = 0; i < 100; ++i) {
        received = receiver.recv_from(buf, sizeof(buf), from);
        if (received > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(received, static_cast<ssize_t>(sizeof(msg)));
    EXPECT_STREQ(reinterpret_cast<const char*>(buf), "hello");
}
