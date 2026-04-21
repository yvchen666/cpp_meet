#include "rtp/rtp_jitter_buffer.h"

void RtpJitterBuffer::push(RtpPacket pkt) {
    uint16_t seq = pkt.sequence();
    buf_.emplace(seq, std::move(pkt));

    // 第一个包到达时，初始化 next_seq_
    if (!started_) {
        next_seq_ = seq;
        started_ = true;
    }
}

std::optional<RtpPacket> RtpJitterBuffer::pop_next() {
    if (buf_.empty()) return std::nullopt;

    auto it = buf_.find(next_seq_);
    if (it == buf_.end()) return std::nullopt;

    RtpPacket pkt = std::move(it->second);
    buf_.erase(it);
    ++next_seq_;  // uint16_t 自动 wrap-around
    return pkt;
}

size_t RtpJitterBuffer::size() const {
    return buf_.size();
}
