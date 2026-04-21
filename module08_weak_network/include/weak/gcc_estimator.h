#pragma once
#include <chrono>
#include <deque>
#include <cstdint>

// GCC 带宽估算器（简化版 Google Congestion Control）
// 基于延迟梯度 trendline + AIMD
class GccEstimator {
public:
    GccEstimator();

    // 报告一个 RTP 包的到达延迟梯度（ms/packet）
    // send_time_ms: 发送时间（从 RTP 扩展头或应用层估算）
    // recv_time_ms: 接收时间
    void on_packet_arrival(int64_t send_time_ms, int64_t recv_time_ms, size_t bytes);

    // 每 100ms 调用一次，返回估算带宽（bps）
    int64_t estimated_bitrate_bps() const { return estimated_bps_; }

private:
    // Trendline 过滤器（线性回归延迟梯度）
    struct Sample { double x, y; };
    std::deque<Sample> trendline_buf_;
    static constexpr int TRENDLINE_WIN = 20;

    double compute_trendline() const;

    enum class State { Normal, Overuse, Underuse };
    State state_{State::Normal};

    int64_t estimated_bps_{1'000'000};  // 初始 1Mbps
    int64_t last_ref_ms_{0};
};
