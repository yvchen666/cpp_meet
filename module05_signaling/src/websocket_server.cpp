#include "sig/websocket_server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <cassert>

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// -----------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// -----------------------------------------------------------------------
// WebSocketServer
// -----------------------------------------------------------------------

WebSocketServer::WebSocketServer(uint16_t port) : port_(port) {}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    // 创建 TCP socket
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");

    if (::listen(server_fd_, 16) < 0)
        throw std::runtime_error("listen() failed");

    set_nonblocking(server_fd_);

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0)
        throw std::runtime_error("epoll_create1() failed");

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    running_ = true;
    thread_ = std::thread(&WebSocketServer::run_loop, this);
}

void WebSocketServer::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
    if (server_fd_ >= 0) { ::close(server_fd_); server_fd_ = -1; }
    if (epoll_fd_ >= 0)  { ::close(epoll_fd_);  epoll_fd_  = -1; }
}

void WebSocketServer::run_loop() {
    constexpr int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];

    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100 /*ms*/);
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == server_fd_) {
                accept_client();
            } else {
                handle_client(fd);
            }
        }
    }

    // 关闭所有连接
    std::lock_guard<std::mutex> lk(conns_mutex_);
    for (auto& [fd, _] : conns_) ::close(fd);
    conns_.clear();
}

void WebSocketServer::accept_client() {
    while (true) {
        sockaddr_in peer{};
        socklen_t   len = sizeof(peer);
        int cfd = ::accept(server_fd_, reinterpret_cast<sockaddr*>(&peer), &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        set_nonblocking(cfd);

        epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLET;
        ev.data.fd = cfd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, cfd, &ev);

        {
            std::lock_guard<std::mutex> lk(conns_mutex_);
            conns_[cfd] = WsConnection{cfd};
        }
    }
}

void WebSocketServer::handle_client(int fd) {
    // 读取数据
    char buf[4096];
    while (true) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            // 错误，断开
            goto disconnect;
        }
        if (n == 0) goto disconnect;

        {
            std::lock_guard<std::mutex> lk(conns_mutex_);
            auto it = conns_.find(fd);
            if (it == conns_.end()) return;
            WsConnection& conn = it->second;

            if (!conn.handshake_done) {
                conn.recv_buf.append(buf, n);
                if (conn.recv_buf.find("\r\n\r\n") != std::string::npos) {
                    if (!do_ws_handshake(conn)) goto disconnect;
                    conn.handshake_done = true;
                    if (on_connect_) on_connect_(fd);
                }
            } else {
                conn.ws_buf.insert(conn.ws_buf.end(),
                                   reinterpret_cast<uint8_t*>(buf),
                                   reinterpret_cast<uint8_t*>(buf) + n);
                parse_ws_frames(conn);
            }
        }
        continue;

    disconnect:
        {
            std::lock_guard<std::mutex> lk(conns_mutex_);
            conns_.erase(fd);
        }
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        ::close(fd);
        if (on_disconnect_) on_disconnect_(fd);
        return;
    }
    return;
}

// -----------------------------------------------------------------------
// WebSocket 握手
// -----------------------------------------------------------------------

bool WebSocketServer::do_ws_handshake(WsConnection& conn) {
    const std::string& req = conn.recv_buf;

    // 找 Sec-WebSocket-Key
    const std::string key_header = "Sec-WebSocket-Key:";
    auto pos = req.find(key_header);
    if (pos == std::string::npos) return false;
    pos += key_header.size();
    // 跳过空格
    while (pos < req.size() && req[pos] == ' ') ++pos;
    auto end = req.find("\r\n", pos);
    if (end == std::string::npos) return false;
    std::string ws_key = req.substr(pos, end - pos);

    std::string accept = sha1_base64(ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "\r\n";

    ssize_t sent = ::write(conn.fd, response.data(), response.size());
    return sent == static_cast<ssize_t>(response.size());
}

// -----------------------------------------------------------------------
// WebSocket 帧解析
// -----------------------------------------------------------------------

void WebSocketServer::parse_ws_frames(WsConnection& conn) {
    auto& buf = conn.ws_buf;

    while (buf.size() >= 2) {
        uint8_t b0 = buf[0];
        uint8_t b1 = buf[1];

        // bool fin    = (b0 & 0x80) != 0;
        uint8_t opcode = b0 & 0x0F;
        bool masked    = (b1 & 0x80) != 0;
        uint64_t payload_len = b1 & 0x7F;

        size_t header_size = 2;
        if (payload_len == 126) {
            if (buf.size() < 4) return;
            payload_len = (static_cast<uint64_t>(buf[2]) << 8) | buf[3];
            header_size = 4;
        } else if (payload_len == 127) {
            if (buf.size() < 10) return;
            payload_len = 0;
            for (int i = 0; i < 8; ++i)
                payload_len = (payload_len << 8) | buf[2 + i];
            header_size = 10;
        }

        size_t mask_size = masked ? 4 : 0;
        size_t total = header_size + mask_size + payload_len;
        if (buf.size() < total) return;

        // 解码 payload
        std::string payload(payload_len, '\0');
        if (masked) {
            uint8_t mask[4];
            for (int i = 0; i < 4; ++i) mask[i] = buf[header_size + i];
            for (uint64_t i = 0; i < payload_len; ++i)
                payload[i] = buf[header_size + 4 + i] ^ mask[i % 4];
        } else {
            for (uint64_t i = 0; i < payload_len; ++i)
                payload[i] = buf[header_size + i];
        }

        // 消费帧
        buf.erase(buf.begin(), buf.begin() + total);

        if (opcode == 0x1) {
            // 文本帧
            if (on_message_) on_message_(conn.fd, payload);
        } else if (opcode == 0x8) {
            // Close 帧：关闭连接
            ::close(conn.fd);
            return;
        } else if (opcode == 0x9) {
            // Ping → Pong
            // pong frame: FIN=1, opcode=0xA
            uint8_t pong[2] = {0x8A, static_cast<uint8_t>(payload_len & 0x7F)};
            ::write(conn.fd, pong, 2);
            if (!payload.empty())
                ::write(conn.fd, payload.data(), payload.size());
        }
        // opcode 0xA (pong), continuation etc. → ignore
    }
}

// -----------------------------------------------------------------------
// build_ws_frame
// -----------------------------------------------------------------------

std::string WebSocketServer::build_ws_frame(const std::string& msg) {
    std::string frame;
    frame.push_back(static_cast<char>(0x81)); // FIN=1, opcode=1 (text)

    size_t len = msg.size();
    if (len < 126) {
        frame.push_back(static_cast<char>(len));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        frame.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<char>((len >> (8 * i)) & 0xFF));
    }
    frame.append(msg);
    return frame;
}

// -----------------------------------------------------------------------
// send / broadcast
// -----------------------------------------------------------------------

void WebSocketServer::send(int conn_id, const std::string& msg) {
    std::string frame = build_ws_frame(msg);
    std::lock_guard<std::mutex> lk(conns_mutex_);
    auto it = conns_.find(conn_id);
    if (it != conns_.end()) {
        ::write(conn_id, frame.data(), frame.size());
    }
}

void WebSocketServer::broadcast(const std::string& msg) {
    std::string frame = build_ws_frame(msg);
    std::lock_guard<std::mutex> lk(conns_mutex_);
    for (auto& [fd, conn] : conns_) {
        if (conn.handshake_done)
            ::write(fd, frame.data(), frame.size());
    }
}

// -----------------------------------------------------------------------
// SHA1 + Base64（OpenSSL）
// -----------------------------------------------------------------------

std::string WebSocketServer::sha1_base64(const std::string& input) {
    unsigned char sha1_hash[SHA_DIGEST_LENGTH]; // 20 bytes
    SHA1(reinterpret_cast<const unsigned char*>(input.data()),
         input.size(), sha1_hash);

    // Base64 encode via OpenSSL BIO
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // 不加换行
    BIO_write(b64, sha1_hash, SHA_DIGEST_LENGTH);
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}
