#include "weak/fec.h"
#include <algorithm>
#include <cstring>

// ──────────────────────────────────────────────
// XorFecEncoder
// ──────────────────────────────────────────────

XorFecEncoder::XorFecEncoder(int group_size)
    : group_size_(group_size) {}

std::optional<RtpPacket> XorFecEncoder::feed(const RtpPacket& pkt) {
    const uint8_t* payload = pkt.payload();
    size_t         plen    = pkt.payload_size();

    if (count_ == 0) {
        base_seq_ = pkt.sequence();
        xor_buf_.assign(payload, payload + plen);
    } else {
        if (xor_buf_.size() < plen) xor_buf_.resize(plen, 0);
        for (size_t i = 0; i < plen; ++i) {
            xor_buf_[i] ^= payload[i];
        }
    }
    ++count_;

    if (count_ == group_size_) {
        RtpPacket fec_pkt;
        // PT=127, timestamp 存储 base_seq 供解码端识别 group
        fec_pkt.build(127, fec_seq_++,
                      static_cast<uint32_t>(base_seq_),
                      fec_ssrc_,
                      xor_buf_.data(), xor_buf_.size());
        count_ = 0;
        xor_buf_.clear();
        return fec_pkt;
    }
    return std::nullopt;
}

// ──────────────────────────────────────────────
// XorFecDecoder
// ──────────────────────────────────────────────

XorFecDecoder::XorFecDecoder(int group_size)
    : group_size_(group_size) {}

// 内联恢复逻辑：返回恢复的包（如有），同时清理 group
std::optional<RtpPacket> XorFecDecoder::feed(const RtpPacket& pkt, bool is_fec) {
    uint16_t base_seq;

    if (is_fec) {
        base_seq = static_cast<uint16_t>(pkt.timestamp() & 0xFFFF);
        auto& g = groups_[base_seq];
        if (g.pkts.empty()) g.pkts.resize(group_size_);
        g.fec = pkt;
    } else {
        uint16_t seq = pkt.sequence();
        base_seq = static_cast<uint16_t>((seq / group_size_) * group_size_);
        auto& g = groups_[base_seq];
        if (g.pkts.empty()) g.pkts.resize(group_size_);

        int idx = static_cast<int>(seq - base_seq);
        if (idx >= 0 && idx < group_size_ && !g.pkts[idx].has_value()) {
            g.pkts[idx] = pkt;
            ++g.received;
        }
    }

    // 尝试恢复：找 group 中只缺1个包且有 FEC 的情况
    for (auto it = groups_.begin(); it != groups_.end(); ) {
        auto& [bs, grp] = *it;
        if (!grp.fec.has_value() || static_cast<int>(grp.pkts.size()) != group_size_) {
            ++it; continue;
        }

        int missing_idx = -1, missing_cnt = 0;
        for (int i = 0; i < group_size_; ++i) {
            if (!grp.pkts[i].has_value()) { ++missing_cnt; missing_idx = i; }
        }

        if (missing_cnt != 1 || missing_idx < 0) { ++it; continue; }

        // XOR 恢复
        const RtpPacket& fec = grp.fec.value();
        std::vector<uint8_t> recovered(fec.payload(), fec.payload() + fec.payload_size());
        for (int i = 0; i < group_size_; ++i) {
            if (i == missing_idx) continue;
            const RtpPacket& p = grp.pkts[i].value();
            size_t plen = p.payload_size();
            if (recovered.size() < plen) recovered.resize(plen, 0);
            for (size_t j = 0; j < plen; ++j) recovered[j] ^= p.payload()[j];
        }

        // 从已有包推断 PT/SSRC/TS
        uint8_t  pt = 96; uint32_t ts = 0; uint32_t ssrc = 0;
        for (int i = 0; i < group_size_; ++i) {
            if (grp.pkts[i].has_value()) {
                pt = grp.pkts[i]->payload_type();
                ts = grp.pkts[i]->timestamp();
                ssrc = grp.pkts[i]->ssrc();
                break;
            }
        }

        uint16_t rec_seq = static_cast<uint16_t>(bs + missing_idx);
        RtpPacket rec_pkt;
        rec_pkt.build(pt, rec_seq, ts, ssrc, recovered.data(), recovered.size());

        it = groups_.erase(it);
        return rec_pkt;
    }

    return std::nullopt;
}
