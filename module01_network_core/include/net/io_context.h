#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <atomic>

namespace net {

class IoContext {
public:
    using Callback = std::function<void(int fd, uint32_t events)>;

    IoContext();
    ~IoContext();

    // Non-copyable, non-movable
    IoContext(const IoContext&) = delete;
    IoContext& operator=(const IoContext&) = delete;

    // Register fd with epoll for given events; callback is invoked on activity
    void register_fd(int fd, uint32_t events, Callback cb);

    // Remove fd from epoll
    void unregister_fd(int fd);

    // Block and run the epoll event loop (level-triggered)
    void run();

    // Wake up and stop the loop
    void stop();

private:
    int epoll_fd_;
    int event_fd_;
    std::atomic<bool> running_;
    std::unordered_map<int, Callback> callbacks_;
};

} // namespace net
