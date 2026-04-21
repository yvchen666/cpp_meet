#include "net/udp_socket.h"

#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace net {

UdpSocket::UdpSocket() : fd_(-1) {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error(std::string("socket() failed: ") + strerror(errno));
    }

    // Set non-blocking
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("fcntl(F_GETFL) failed: ") + strerror(errno));
    }
    if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("fcntl(F_SETFL) failed: ") + strerror(errno));
    }
}

UdpSocket::~UdpSocket() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool UdpSocket::bind(uint16_t port) {
    int opt = 1;
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    return ::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0;
}

ssize_t UdpSocket::send_to(const uint8_t* data, size_t len, const sockaddr_in& addr) {
    return ::sendto(fd_, data, len, 0,
                    reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

ssize_t UdpSocket::recv_from(uint8_t* buf, size_t len, sockaddr_in& from) {
    socklen_t from_len = sizeof(from);
    return ::recvfrom(fd_, buf, len, 0,
                      reinterpret_cast<sockaddr*>(&from), &from_len);
}

} // namespace net
