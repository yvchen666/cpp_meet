#include "video/rtp_video_packetizer.h"
#include <cstring>

// 跳过 Annex B start code，返回裸 NAL 数据指针和长度
static const uint8_t* skip_start_code(const uint8_t* data, size_t size, size_t& out_size) {
    if (size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        out_size = size - 4;
        return data + 4;
    }
    if (size >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1) {
        out_size = size - 3;
        return data + 3;
    }
    out_size = size;
    return data;
}

std::vector<RtpPacket> RtpVideoPacketizer::packetize(
    const NalUnit& nal,
    uint8_t payload_type,
    uint16_t& seq,
    uint32_t timestamp,
    uint32_t ssrc)
{
    size_t nal_size = 0;
    const uint8_t* nal_data = skip_start_code(nal.data.data(), nal.data.size(), nal_size);

    if (nal_size == 0) return {};

    if (nal_size <= MAX_RTP_PAYLOAD) {
        // Single NAL Unit packet
        RtpPacket pkt;
        pkt.build(payload_type, seq++, timestamp, ssrc,
                  nal_data, nal_size, /*marker=*/true);
        return {pkt};
    }

    return make_fua_packets(nal_data, nal_size, payload_type, seq, timestamp, ssrc);
}

std::vector<RtpPacket> RtpVideoPacketizer::make_fua_packets(
    const uint8_t* nal_data, size_t nal_size,
    uint8_t payload_type, uint16_t& seq,
    uint32_t timestamp, uint32_t ssrc)
{
    // RFC 6184 Section 5.8: FU-A
    // FU indicator byte: F | NRI | Type(28)
    // FU header byte:    S | E | R | nal_type
    uint8_t nal_header = nal_data[0];
    uint8_t fu_indicator = (nal_header & 0xE0) | 28;  // NRI + type=28
    uint8_t nal_type     = nal_header & 0x1F;

    const size_t max_payload = MAX_RTP_PAYLOAD - 2;  // 2 bytes header overhead
    const uint8_t* src = nal_data + 1;  // skip original NAL header
    size_t remaining = nal_size - 1;

    std::vector<RtpPacket> result;
    bool first = true;

    while (remaining > 0) {
        size_t chunk = std::min(remaining, max_payload);
        bool last = (chunk == remaining);

        uint8_t fu_header = nal_type;
        if (first) fu_header |= 0x80;  // S bit
        if (last)  fu_header |= 0x40;  // E bit

        std::vector<uint8_t> payload(2 + chunk);
        payload[0] = fu_indicator;
        payload[1] = fu_header;
        std::memcpy(payload.data() + 2, src, chunk);

        RtpPacket pkt;
        pkt.build(payload_type, seq++, timestamp, ssrc,
                  payload.data(), payload.size(), /*marker=*/last);
        result.push_back(std::move(pkt));

        src       += chunk;
        remaining -= chunk;
        first = false;
    }
    return result;
}
