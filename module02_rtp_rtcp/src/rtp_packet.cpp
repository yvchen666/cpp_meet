#include "rtp/rtp_packet.h"
#include <stdexcept>

// RTP 固定头长度
static constexpr size_t kRtpMinHeaderLen = 12;

RtpPacket::RtpPacket(std::vector<uint8_t> data)
    : buf_(std::move(data)), payload_offset_(0)
{
    if (buf_.size() >= kRtpMinHeaderLen) {
        uint8_t cc = buf_[0] & 0x0F;
        payload_offset_ = kRtpMinHeaderLen + cc * 4;
    }
}

bool RtpPacket::parse(const uint8_t* data, size_t len) {
    if (len < kRtpMinHeaderLen) return false;

    // version 必须为 2
    uint8_t version = (data[0] >> 6) & 0x03;
    if (version != 2) return false;

    uint8_t cc = data[0] & 0x0F;
    size_t header_len = kRtpMinHeaderLen + cc * 4;
    if (len < header_len) return false;

    // 如果有扩展头，跳过
    bool has_ext = (data[0] >> 4) & 0x01;
    size_t offset = header_len;
    if (has_ext) {
        if (offset + 4 > len) return false;
        uint16_t ext_len = ((uint16_t)data[offset + 2] << 8) | data[offset + 3];
        offset += 4 + ext_len * 4;
        if (offset > len) return false;
    }

    buf_.assign(data, data + len);
    payload_offset_ = offset;
    return true;
}

void RtpPacket::build(uint8_t payload_type, uint16_t seq, uint32_t timestamp,
                      uint32_t ssrc, const uint8_t* payload, size_t payload_len,
                      bool marker)
{
    buf_.resize(kRtpMinHeaderLen + payload_len);

    // Byte 0: V=2, P=0, X=0, CC=0
    buf_[0] = 0x80;
    // Byte 1: M bit + PT
    buf_[1] = (marker ? 0x80 : 0x00) | (payload_type & 0x7F);
    // Bytes 2-3: sequence number (big-endian)
    buf_[2] = (seq >> 8) & 0xFF;
    buf_[3] = seq & 0xFF;
    // Bytes 4-7: timestamp (big-endian)
    buf_[4] = (timestamp >> 24) & 0xFF;
    buf_[5] = (timestamp >> 16) & 0xFF;
    buf_[6] = (timestamp >>  8) & 0xFF;
    buf_[7] = timestamp & 0xFF;
    // Bytes 8-11: SSRC (big-endian)
    buf_[8]  = (ssrc >> 24) & 0xFF;
    buf_[9]  = (ssrc >> 16) & 0xFF;
    buf_[10] = (ssrc >>  8) & 0xFF;
    buf_[11] = ssrc & 0xFF;

    payload_offset_ = kRtpMinHeaderLen;
    if (payload && payload_len > 0) {
        std::memcpy(buf_.data() + payload_offset_, payload, payload_len);
    }
}

uint8_t RtpPacket::version() const {
    if (buf_.size() < 1) return 0;
    return (buf_[0] >> 6) & 0x03;
}

bool RtpPacket::padding() const {
    if (buf_.size() < 1) return false;
    return (buf_[0] >> 5) & 0x01;
}

bool RtpPacket::extension() const {
    if (buf_.size() < 1) return false;
    return (buf_[0] >> 4) & 0x01;
}

uint8_t RtpPacket::cc() const {
    if (buf_.size() < 1) return 0;
    return buf_[0] & 0x0F;
}

bool RtpPacket::marker() const {
    if (buf_.size() < 2) return false;
    return (buf_[1] >> 7) & 0x01;
}

uint8_t RtpPacket::payload_type() const {
    if (buf_.size() < 2) return 0;
    return buf_[1] & 0x7F;
}

uint16_t RtpPacket::sequence() const {
    if (buf_.size() < 4) return 0;
    return ((uint16_t)buf_[2] << 8) | buf_[3];
}

uint32_t RtpPacket::timestamp() const {
    if (buf_.size() < 8) return 0;
    return ((uint32_t)buf_[4] << 24) |
           ((uint32_t)buf_[5] << 16) |
           ((uint32_t)buf_[6] <<  8) |
            (uint32_t)buf_[7];
}

uint32_t RtpPacket::ssrc() const {
    if (buf_.size() < 12) return 0;
    return ((uint32_t)buf_[8]  << 24) |
           ((uint32_t)buf_[9]  << 16) |
           ((uint32_t)buf_[10] <<  8) |
            (uint32_t)buf_[11];
}

const uint8_t* RtpPacket::payload() const {
    if (payload_offset_ >= buf_.size()) return nullptr;
    return buf_.data() + payload_offset_;
}

size_t RtpPacket::payload_size() const {
    if (payload_offset_ >= buf_.size()) return 0;
    return buf_.size() - payload_offset_;
}

const uint8_t* RtpPacket::data() const {
    return buf_.data();
}

size_t RtpPacket::size() const {
    return buf_.size();
}
