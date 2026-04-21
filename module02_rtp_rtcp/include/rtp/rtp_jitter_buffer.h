#pragma once
#include "rtp_packet.h"
#include <map>
#include <optional>

// 按序号排序的 RTP 包缓冲区（基础版，供后续 module08 扩展）
class RtpJitterBuffer {
public:
    void push(RtpPacket pkt);
    std::optional<RtpPacket> pop_next(); // 按序号取出
    size_t size() const;
private:
    std::map<uint16_t, RtpPacket> buf_;
    uint16_t next_seq_{0};
    bool started_{false};
};
