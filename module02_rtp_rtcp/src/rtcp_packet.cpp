#include "rtp/rtcp_packet.h"
#include <algorithm>

// ─── 工具函数：大端序写入 ───────────────────────────────────────────────────

static void write_u16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back(v & 0xFF);
}

static void write_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >>  8) & 0xFF);
    buf.push_back(v & 0xFF);
}

static uint16_t read_u16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t read_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
            (uint32_t)p[3];
}

// ─── RtcpSR ───────────────────────────────────────────────────────────────

// SR 格式（RFC 3550 Section 6.4.1）：
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|    RC   |   PT=SR=200   |             length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         SSRC of sender                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |              NTP timestamp, most significant word             |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |             NTP timestamp, least significant word             |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         RTP timestamp                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     sender's packet count                     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      sender's octet count                     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// length 字段 = (总字节数 / 4) - 1，SR 无 report block 时 = 6

std::vector<uint8_t> RtcpSR::serialize() const {
    std::vector<uint8_t> buf;
    buf.reserve(28);

    // Byte 0: V=2, P=0, RC=0
    buf.push_back(0x80);
    // Byte 1: PT=200
    buf.push_back(200);
    // Bytes 2-3: length = 6 (28 bytes / 4 - 1)
    write_u16(buf, 6);

    write_u32(buf, ssrc);
    write_u32(buf, ntp_sec);
    write_u32(buf, ntp_frac);
    write_u32(buf, rtp_timestamp);
    write_u32(buf, packet_count);
    write_u32(buf, octet_count);

    return buf;
}

bool RtcpSR::parse(const uint8_t* data, size_t len) {
    // 最小长度：4 (common header) + 24 (SR fields) = 28
    if (len < 28) return false;

    uint8_t version = (data[0] >> 6) & 0x03;
    if (version != 2) return false;

    uint8_t pt = data[1];
    if (pt != 200) return false;

    // length 字段（以 32-bit words 为单位，不含首字），实际字节 = (length+1)*4
    uint16_t length_words = read_u16(data + 2);
    size_t expected = (length_words + 1) * 4;
    if (len < expected) return false;

    ssrc          = read_u32(data + 4);
    ntp_sec       = read_u32(data + 8);
    ntp_frac      = read_u32(data + 12);
    rtp_timestamp = read_u32(data + 16);
    packet_count  = read_u32(data + 20);
    octet_count   = read_u32(data + 24);

    return true;
}

// ─── RtcpNack ─────────────────────────────────────────────────────────────

// RTPFB NACK 格式（RFC 4585 Section 6.2.1）：
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P| FMT=1  |    PT=205     |             length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                  SSRC of packet sender                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                  SSRC of media source                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |            PID (FSN)          |             BLP               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// ...（可有多个 PID+BLP 对）

// BLP 编码：FSN 是第一个丢失序列号，BLP 的第 i 位（i=0 最低位）代表 FSN+i+1
// 若该位为 1，表示对应序列号也丢失

std::vector<uint8_t> RtcpNack::serialize() const {
    // 将 lost_seqs 压缩成 FSN+BLP 对
    struct NackPair { uint16_t fsn; uint16_t blp; };
    std::vector<NackPair> pairs;

    // 对序列号排序（处理 wrap-around：简单起见按数值排序，实际使用时应在外部保证顺序合理）
    std::vector<uint16_t> seqs = lost_seqs;
    std::sort(seqs.begin(), seqs.end());
    seqs.erase(std::unique(seqs.begin(), seqs.end()), seqs.end());

    size_t i = 0;
    while (i < seqs.size()) {
        NackPair p;
        p.fsn = seqs[i];
        p.blp = 0;
        // 将 FSN+1 到 FSN+16 范围内的其他丢失序列号打包进 BLP
        for (size_t j = i + 1; j < seqs.size(); ++j) {
            int diff = (int)(seqs[j]) - (int)(p.fsn);
            if (diff >= 1 && diff <= 16) {
                p.blp |= (uint16_t)(1 << (diff - 1));
            } else if (diff > 16) {
                break;
            }
        }
        pairs.push_back(p);
        // 跳过已经被当前 BLP 覆盖的序列号
        while (i + 1 < seqs.size()) {
            int diff = (int)(seqs[i + 1]) - (int)(p.fsn);
            if (diff <= 16) {
                ++i;
            } else {
                break;
            }
        }
        ++i;
    }

    std::vector<uint8_t> buf;
    // common header (4) + sender_ssrc (4) + media_ssrc (4) + pairs * 4
    size_t total = 12 + pairs.size() * 4;
    buf.reserve(total);

    // Byte 0: V=2, P=0, FMT=1
    buf.push_back(0x81);
    // Byte 1: PT=205
    buf.push_back(205);
    // length = (total / 4) - 1
    uint16_t length_words = static_cast<uint16_t>(total / 4 - 1);
    write_u16(buf, length_words);

    write_u32(buf, sender_ssrc);
    write_u32(buf, media_ssrc);

    for (const auto& p : pairs) {
        write_u16(buf, p.fsn);
        write_u16(buf, p.blp);
    }

    return buf;
}

bool RtcpNack::parse(const uint8_t* data, size_t len) {
    // 最小长度：4 (common header) + 4 (sender_ssrc) + 4 (media_ssrc) = 12
    if (len < 12) return false;

    uint8_t version = (data[0] >> 6) & 0x03;
    if (version != 2) return false;

    uint8_t fmt = data[0] & 0x1F;
    if (fmt != 1) return false;

    uint8_t pt = data[1];
    if (pt != 205) return false;

    uint16_t length_words = read_u16(data + 2);
    size_t expected = (length_words + 1) * 4;
    if (len < expected) return false;

    sender_ssrc = read_u32(data + 4);
    media_ssrc  = read_u32(data + 8);

    lost_seqs.clear();
    size_t offset = 12;
    while (offset + 4 <= expected) {
        uint16_t fsn = read_u16(data + offset);
        uint16_t blp = read_u16(data + offset + 2);
        offset += 4;

        lost_seqs.push_back(fsn);
        for (int i = 0; i < 16; ++i) {
            if (blp & (1 << i)) {
                lost_seqs.push_back(static_cast<uint16_t>(fsn + i + 1));
            }
        }
    }

    return true;
}
