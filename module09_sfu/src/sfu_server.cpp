#include "sfu/sfu_server.h"
#include <sys/epoll.h>
#include <iostream>

SfuServer::SfuServer(uint16_t udp_port)
    : udp_port_(udp_port)
{}

SfuServer::~SfuServer() {
    stop();
}

void SfuServer::start() {
    if (!udp_.bind(udp_port_)) {
        std::cerr << "[SfuServer] Failed to bind UDP port " << udp_port_ << "\n";
        return;
    }

    io_.register_fd(udp_.fd(), EPOLLIN, [this](int /*fd*/, uint32_t /*events*/) {
        uint8_t buf[65536];
        sockaddr_in from{};
        ssize_t n = udp_.recv_from(buf, sizeof(buf), from);
        if (n > 0) {
            on_udp_packet(buf, static_cast<size_t>(n), from);
        }
    });

    io_thread_ = std::thread([this]() {
        io_.run();
    });
}

void SfuServer::stop() {
    io_.stop();
    if (io_thread_.joinable())
        io_thread_.join();
}

SfuRoom* SfuServer::get_or_create_room(const std::string& room_id) {
    std::lock_guard<std::mutex> lk(rooms_mutex_);
    auto it = rooms_.find(room_id);
    if (it != rooms_.end())
        return it->second.get();

    auto send_fn = [this](int /*peer_id*/, const uint8_t* /*data*/, size_t /*len*/) {
        // 实际项目中需要维护 peer_id -> sockaddr_in 映射，此处简化
    };

    auto room = std::make_unique<SfuRoom>(room_id, std::move(send_fn));
    SfuRoom* ptr = room.get();
    rooms_[room_id] = std::move(room);
    return ptr;
}

void SfuServer::on_udp_packet(const uint8_t* data, size_t len, const sockaddr_in& /*from*/) {
    // 简化路由：实际项目中需要解析头部来确定 room 和 peer
    // 此处仅演示框架
    if (len < 4) return;
    // TODO: 根据协议头确定 room_id 和 peer_id，调用 room->route_rtp()
}
