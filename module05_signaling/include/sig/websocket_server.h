#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>

struct WsConnection {
    int fd;
    bool handshake_done{false};
    std::string recv_buf;        // HTTP 升级握手缓冲
    std::vector<uint8_t> ws_buf; // WebSocket 帧缓冲
};

// 简单的 WebSocket 服务器（仅支持文本帧，RFC 6455 子集）
class WebSocketServer {
public:
    using MessageCb = std::function<void(int conn_id, const std::string& msg)>;
    using ConnCb    = std::function<void(int conn_id)>;

    WebSocketServer(uint16_t port);
    ~WebSocketServer();

    void on_message(MessageCb cb) { on_message_ = cb; }
    void on_connect(ConnCb cb)    { on_connect_ = cb; }
    void on_disconnect(ConnCb cb) { on_disconnect_ = cb; }

    void start();  // 非阻塞，启动后台线程
    void stop();

    void send(int conn_id, const std::string& msg);
    void broadcast(const std::string& msg);

private:
    uint16_t port_;
    int server_fd_{-1};
    int epoll_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex conns_mutex_;
    std::unordered_map<int, WsConnection> conns_;

    MessageCb on_message_;
    ConnCb    on_connect_;
    ConnCb    on_disconnect_;

    void run_loop();
    void accept_client();
    void handle_client(int fd);
    bool do_ws_handshake(WsConnection& conn);
    void parse_ws_frames(WsConnection& conn);
    std::string build_ws_frame(const std::string& msg);

    // SHA1 用于 WebSocket 握手
    static std::string sha1_base64(const std::string& input);
};
