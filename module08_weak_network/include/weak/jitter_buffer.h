#pragma once
#include <rtp/rtp_packet.h>
#include <map>
#include <optional>
#include <chrono>

// 自适应 JitterBuffer（基于时间戳排序）
class JitterBuffer {
public:
    // target_delay_ms: 初始目标延迟（毫秒）
    explicit JitterBuffer(int target_delay_ms = 100);

    void push(RtpPacket pkt);

    // 根据当前时间决定是否输出包
    std::optional<RtpPacket> pop(std::chrono::steady_clock::time_point now);

    // 更新平滑抖动估计（ms）
    double smoothed_jitter_ms() const { return smoothed_jitter_ms_; }

private:
    int target_delay_ms_;
    std::map<uint16_t, RtpPacket> buf_;
    double smoothed_jitter_ms_{0.0};
    std::chrono::steady_clock::time_point last_arrival_;
    bool started_{false};
};
