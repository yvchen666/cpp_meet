#pragma once
#include <cstdint>
#include <vector>

// RTCP Sender Report (PT=200) 和 Receiver Report (PT=201)
// 以及 RTCP NACK (RTPFB, PT=205, FMT=1)

struct RtcpSR {
    uint32_t ssrc;
    uint32_t ntp_sec;
    uint32_t ntp_frac;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;

    std::vector<uint8_t> serialize() const;
    bool parse(const uint8_t* data, size_t len);
};

struct RtcpNack {
    uint32_t sender_ssrc;
    uint32_t media_ssrc;
    std::vector<uint16_t> lost_seqs;  // 展开后的序列号列表

    std::vector<uint8_t> serialize() const;
    bool parse(const uint8_t* data, size_t len);
};
