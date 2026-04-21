#include "weak/jitter_buffer.h"
#include <cmath>

JitterBuffer::JitterBuffer(int target_delay_ms)
    : target_delay_ms_(target_delay_ms) {}

void JitterBuffer::push(RtpPacket pkt) {
    auto now = std::chrono::steady_clock::now();

    if (started_) {
        // 计算到达间隔抖动
        auto delta = std::chrono::duration<double, std::milli>(now - last_arrival_).count();
        // RFC 3550 抖动估算（指数移动平均）
        double diff = std::abs(delta - 20.0);  // 期望 20ms 间隔
        smoothed_jitter_ms_ += (diff - smoothed_jitter_ms_) / 16.0;
    }
    last_arrival_ = now;
    started_ = true;

    uint16_t seq = pkt.sequence();
    buf_.emplace(seq, std::move(pkt));
}

std::optional<RtpPacket> JitterBuffer::pop(std::chrono::steady_clock::time_point now) {
    if (buf_.empty()) return std::nullopt;

    // 只有当缓冲区有足够延迟时才输出
    if (!started_) return std::nullopt;

    auto elapsed = std::chrono::duration<double, std::milli>(now - last_arrival_).count();
    // 已经积累了足够的延迟，或者缓冲区较大
    int effective_delay = target_delay_ms_ + static_cast<int>(smoothed_jitter_ms_ * 2);
    if (elapsed >= static_cast<double>(effective_delay) || buf_.size() > 10) {
        auto it = buf_.begin();
        RtpPacket pkt = std::move(it->second);
        buf_.erase(it);
        return pkt;
    }
    return std::nullopt;
}
