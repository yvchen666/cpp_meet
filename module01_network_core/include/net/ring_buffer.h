#pragma once

#include <atomic>
#include <cstddef>

namespace net {

// Lock-free Single-Producer Single-Consumer ring buffer.
// N must be a power of 2.
template<typename T, size_t N>
class RingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

    // Producer owns head_ (writes it), reads tail_ with relaxed
    alignas(64) std::atomic<size_t> head_{0};
    // Consumer owns tail_ (writes it), reads head_ with relaxed
    alignas(64) std::atomic<size_t> tail_{0};

    T buf_[N];

public:
    // Returns false if full
    bool push(const T& item) {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t next_h = (h + 1) & (N - 1);
        // tail_ is written by consumer with release; we need acquire to see
        // all consumer writes to buf_ slots that were freed
        if (next_h == tail_.load(std::memory_order_acquire)) {
            return false; // full
        }
        buf_[h] = item;
        // Publish the new head to the consumer
        head_.store(next_h, std::memory_order_release);
        return true;
    }

    // Returns false if empty
    bool pop(T& item) {
        const size_t t = tail_.load(std::memory_order_relaxed);
        // head_ is written by producer with release; acquire to see buf_ data
        if (t == head_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        item = buf_[t];
        // Publish the new tail to the producer
        tail_.store((t + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    bool full() const {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return ((h + 1) & (N - 1)) == t;
    }

    // Number of elements currently in the buffer (snapshot, approximate under concurrency)
    size_t size() const {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (h - t + N) & (N - 1);
    }
};

} // namespace net
