#include "net/io_context.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <cstdint>

namespace net {

static constexpr int kMaxEvents = 64;
static constexpr int kEpollTimeoutMs = 1000;

IoContext::IoContext() : epoll_fd_(-1), event_fd_(-1), running_(false) {
    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ < 0) {
        throw std::runtime_error(std::string("epoll_create1() failed: ") + strerror(errno));
    }

    event_fd_ = ::eventfd(0, EFD_NONBLOCK);
    if (event_fd_ < 0) {
        ::close(epoll_fd_);
        throw std::runtime_error(std::string("eventfd() failed: ") + strerror(errno));
    }

    // Register eventfd so stop() can wake up epoll_wait
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = event_fd_;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd_, &ev) < 0) {
        ::close(event_fd_);
        ::close(epoll_fd_);
        throw std::runtime_error(std::string("epoll_ctl(event_fd) failed: ") + strerror(errno));
    }

    // Register a no-op callback for the eventfd
    callbacks_[event_fd_] = [](int /*fd*/, uint32_t /*events*/) {};
}

IoContext::~IoContext() {
    if (event_fd_ >= 0) ::close(event_fd_);
    if (epoll_fd_ >= 0) ::close(epoll_fd_);
}

void IoContext::register_fd(int fd, uint32_t events, Callback cb) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::runtime_error(std::string("epoll_ctl(ADD) failed: ") + strerror(errno));
    }
    callbacks_[fd] = std::move(cb);
}

void IoContext::unregister_fd(int fd) {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    callbacks_.erase(fd);
}

void IoContext::run() {
    running_.store(true, std::memory_order_relaxed);

    epoll_event events[kMaxEvents];

    while (running_.load(std::memory_order_relaxed)) {
        int n = ::epoll_wait(epoll_fd_, events, kMaxEvents, kEpollTimeoutMs);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == event_fd_) {
                // Drain the eventfd counter; stop() has set running_ = false
                uint64_t val;
                ::read(event_fd_, &val, sizeof(val));
                continue;
            }

            auto it = callbacks_.find(fd);
            if (it != callbacks_.end()) {
                it->second(fd, events[i].events);
            }
        }
    }
}

void IoContext::stop() {
    running_.store(false, std::memory_order_relaxed);
    // Write 1 to eventfd to wake up epoll_wait
    uint64_t val = 1;
    ::write(event_fd_, &val, sizeof(val));
}

} // namespace net
