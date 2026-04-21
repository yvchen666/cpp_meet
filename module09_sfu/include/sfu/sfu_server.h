#pragma once
#include "sfu_room.h"
#include <net/io_context.h>
#include <net/udp_socket.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>
#include <thread>

// SFU 服务器：管理多个房间
class SfuServer {
public:
    explicit SfuServer(uint16_t udp_port);
    ~SfuServer();

    void start();
    void stop();

    // 创建或获取房间
    SfuRoom* get_or_create_room(const std::string& room_id);

private:
    uint16_t udp_port_;
    net::UdpSocket udp_;
    net::IoContext io_;
    std::thread io_thread_;
    std::mutex rooms_mutex_;
    std::unordered_map<std::string, std::unique_ptr<SfuRoom>> rooms_;

    void on_udp_packet(const uint8_t* data, size_t len, const sockaddr_in& from);
};
