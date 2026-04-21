#include <gtest/gtest.h>
#include "sig/websocket_server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// -----------------------------------------------------------------------
// 辅助：手动发 HTTP Upgrade + WS 帧
// -----------------------------------------------------------------------

static int connect_to(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// 读取直到找到指定字符串或超时
static std::string read_until(int fd, const std::string& sentinel, int timeout_ms = 2000) {
    std::string buf;
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    char tmp[256];
    while (buf.find(sentinel) == std::string::npos) {
        if (std::chrono::steady_clock::now() > deadline) break;
        ssize_t n = ::read(fd, tmp, sizeof(tmp));
        if (n > 0) buf.append(tmp, n);
        else std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return buf;
}

// 构造一个带掩码的 WS 文本帧（客户端发送必须 mask）
static std::vector<uint8_t> make_ws_text_frame(const std::string& msg) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN=1, opcode=1 (text)
    uint8_t mask_bit = 0x80;
    size_t len = msg.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(mask_bit | len));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<uint8_t>(mask_bit | 126));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    }
    // masking key（固定为 0xDE 0xAD 0xBE 0xEF 以便测试）
    uint8_t masking_key[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    for (int i = 0; i < 4; ++i) frame.push_back(masking_key[i]);
    for (size_t i = 0; i < msg.size(); ++i)
        frame.push_back(static_cast<uint8_t>(msg[i]) ^ masking_key[i % 4]);
    return frame;
}

// -----------------------------------------------------------------------
// TEST: HandshakeAndMessage
// -----------------------------------------------------------------------

TEST(WebSocketServer, HandshakeAndMessage) {
    constexpr uint16_t PORT = 19876;

    WebSocketServer server(PORT);
    std::atomic<bool> got_msg{false};
    std::string received_msg;

    server.on_message([&](int /*conn_id*/, const std::string& msg) {
        received_msg = msg;
        got_msg = true;
    });

    // 1. 启动服务器
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 2. 客户端手动发 HTTP Upgrade 请求
    int cfd = connect_to(PORT);
    ASSERT_GE(cfd, 0) << "connect failed";

    const char* upgrade_req =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ::write(cfd, upgrade_req, strlen(upgrade_req));

    // 3. 验证 101 响应
    std::string resp = read_until(cfd, "\r\n\r\n");
    EXPECT_NE(resp.find("101 Switching Protocols"), std::string::npos)
        << "Expected 101 response, got: " << resp;
    EXPECT_NE(resp.find("Sec-WebSocket-Accept"), std::string::npos);

    // 4. 发送 WS 文本帧 "hello"
    auto frame = make_ws_text_frame("hello");
    ::write(cfd, frame.data(), frame.size());

    // 5. 服务器 on_message 收到 "hello"
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    while (!got_msg && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_TRUE(got_msg.load());
    EXPECT_EQ(received_msg, "hello");

    ::close(cfd);
    server.stop();
}

// -----------------------------------------------------------------------
// TEST: SendToClient
// -----------------------------------------------------------------------

TEST(WebSocketServer, SendToClient) {
    constexpr uint16_t PORT = 19877;

    WebSocketServer server(PORT);
    std::atomic<int> connected_id{-1};

    server.on_connect([&](int conn_id) {
        connected_id = conn_id;
    });

    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int cfd = connect_to(PORT);
    ASSERT_GE(cfd, 0);

    const char* upgrade_req =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ::write(cfd, upgrade_req, strlen(upgrade_req));

    // 等 101
    std::string resp = read_until(cfd, "\r\n\r\n");
    ASSERT_NE(resp.find("101"), std::string::npos);

    // 等服务器 on_connect 触发
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (connected_id == -1 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_NE(connected_id.load(), -1);

    // 服务器向客户端发送消息
    server.send(connected_id, "world");

    // 客户端读取 WS 帧
    std::string raw = read_until(cfd, "world");
    // WS 帧头至少 2 字节，payload = "world"
    EXPECT_NE(raw.find("world"), std::string::npos);

    ::close(cfd);
    server.stop();
}
