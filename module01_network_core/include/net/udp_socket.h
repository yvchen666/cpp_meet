#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>
#include <netinet/in.h>

namespace net {

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    // Non-copyable
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Movable
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    bool bind(uint16_t port);

    ssize_t send_to(const uint8_t* data, size_t len, const sockaddr_in& addr);
    ssize_t recv_from(uint8_t* buf, size_t len, sockaddr_in& from);

    int fd() const { return fd_; }
    bool valid() const { return fd_ >= 0; }

private:
    int fd_;
};

} // namespace net
