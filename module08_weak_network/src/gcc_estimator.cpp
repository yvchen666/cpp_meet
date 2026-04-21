#include "weak/gcc_estimator.h"
#include <cmath>
#include <numeric>

GccEstimator::GccEstimator() = default;

void GccEstimator::on_packet_arrival(int64_t send_time_ms, int64_t recv_time_ms, size_t /*bytes*/) {
    // 计算单向延迟（one-way delay）
    int64_t delay = recv_time_ms - send_time_ms;

    // 加入 trendline 缓冲（x = 包序号，y = 延迟）
    double x = static_cast<double>(trendline_buf_.size());
    double y = static_cast<double>(delay);
    trendline_buf_.push_back({x, y});
    if (static_cast<int>(trendline_buf_.size()) > TRENDLINE_WIN) {
        trendline_buf_.pop_front();
        // 重新编号 x
        for (size_t i = 0; i < trendline_buf_.size(); ++i) {
            trendline_buf_[i].x = static_cast<double>(i);
        }
    }

    if (static_cast<int>(trendline_buf_.size()) < 4) return;

    double slope = compute_trendline();

    // 状态机判断
    constexpr double OVERUSE_THRESH  =  0.5;  // ms/packet 增大阈值
    constexpr double UNDERUSE_THRESH = -0.5;

    State prev_state = state_;
    if (slope > OVERUSE_THRESH) {
        state_ = State::Overuse;
    } else if (slope < UNDERUSE_THRESH) {
        state_ = State::Underuse;
    } else {
        state_ = State::Normal;
    }

    // AIMD 码率控制
    if (state_ == State::Overuse && prev_state != State::Overuse) {
        // 乘性减
        estimated_bps_ = static_cast<int64_t>(estimated_bps_ * 0.85);
        if (estimated_bps_ < 50'000) estimated_bps_ = 50'000;
    } else if (state_ == State::Normal || state_ == State::Underuse) {
        // 加性增（每次调用 +100kbps，调用方应每 100ms 调用一次）
        estimated_bps_ += 100'000;
    }
}

double GccEstimator::compute_trendline() const {
    int n = static_cast<int>(trendline_buf_.size());
    if (n < 2) return 0.0;

    double sum_x = 0, sum_y = 0;
    for (auto& s : trendline_buf_) { sum_x += s.x; sum_y += s.y; }
    double mean_x = sum_x / n;
    double mean_y = sum_y / n;

    double num = 0, den = 0;
    for (auto& s : trendline_buf_) {
        double dx = s.x - mean_x;
        double dy = s.y - mean_y;
        num += dx * dy;
        den += dx * dx;
    }
    if (std::abs(den) < 1e-9) return 0.0;
    return num / den;
}
