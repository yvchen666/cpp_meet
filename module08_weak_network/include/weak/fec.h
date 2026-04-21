#pragma once
#include <rtp/rtp_packet.h>
#include <vector>
#include <optional>
#include <map>

// XOR-FEC（每 group_size 个数据包生成一个冗余包）
class XorFecEncoder {
public:
    explicit XorFecEncoder(int group_size = 5);

    // 输入数据 RTP 包，返回（如果到达group边界）FEC 包
    std::optional<RtpPacket> feed(const RtpPacket& pkt);

private:
    int group_size_;
    int count_{0};
    std::vector<uint8_t> xor_buf_;
    uint16_t base_seq_{0};
    uint32_t fec_ssrc_{0xFEC00000};
    uint16_t fec_seq_{0};
};

class XorFecDecoder {
public:
    explicit XorFecDecoder(int group_size = 5);

    // 喂入数据包或 FEC 包，尝试恢复丢失的包
    // 返回恢复出的 RTP 包（如有）
    std::optional<RtpPacket> feed(const RtpPacket& pkt, bool is_fec);

private:
    int group_size_;
    struct Group {
        std::vector<std::optional<RtpPacket>> pkts;
        std::optional<RtpPacket> fec;
        int received{0};
    };
    std::map<uint16_t, Group> groups_;  // 按 base_seq 索引
};
