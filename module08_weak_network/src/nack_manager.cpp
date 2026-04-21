#include "weak/nack_manager.h"
#include <rtp/rtp_packet.h>

void NackManager::on_receive(uint16_t seq) {
    received_.insert(seq);

    if (!started_) {
        max_seen_ = seq;
        started_  = true;
        return;
    }

    // 更新 max_seen_（使用 RFC 3550 环绕安全比较）
    if (RtpPacket::seq_gt(seq, max_seen_)) {
        // 检测新出现的间隙
        uint16_t prev = max_seen_;
        for (uint16_t s = static_cast<uint16_t>(prev + 1); s != seq; ++s) {
            if (received_.find(s) == received_.end()) {
                missing_[s] = std::chrono::steady_clock::now();
            }
        }
        max_seen_ = seq;
    }

    // 收到了之前认为 missing 的包，从 missing_ 中移除
    auto it = missing_.find(seq);
    if (it != missing_.end()) {
        missing_.erase(it);
    }
}

std::vector<uint16_t> NackManager::get_nack_list(std::chrono::steady_clock::time_point now) {
    constexpr int NACK_TIMEOUT_MS = 100;
    std::vector<uint16_t> result;

    for (auto& [seq, first_seen] : missing_) {
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - first_seen).count();
        if (elapsed_ms >= NACK_TIMEOUT_MS) {
            result.push_back(seq);
        }
    }
    return result;
}
