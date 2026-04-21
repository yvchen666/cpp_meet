#pragma once
#include "h264_encoder.h"
#include <rtp/rtp_packet.h>
#include <vector>

// RTP H.264 打包（RFC 6184）
// 支持 Single NAL Unit 和 FU-A 分片
class RtpVideoPacketizer {
public:
    static constexpr size_t MAX_RTP_PAYLOAD = 1200;

    // 将 NAL unit 打包为 RTP 包列表
    // payload_type: 典型为 96
    std::vector<RtpPacket> packetize(
        const NalUnit& nal,
        uint8_t payload_type,
        uint16_t& seq,
        uint32_t timestamp,
        uint32_t ssrc);

private:
    std::vector<RtpPacket> make_fua_packets(
        const uint8_t* nal_data, size_t nal_size,
        uint8_t payload_type, uint16_t& seq,
        uint32_t timestamp, uint32_t ssrc);
};
