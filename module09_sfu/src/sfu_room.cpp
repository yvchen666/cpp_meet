#include "sfu/sfu_room.h"
#include <cstring>
#include <algorithm>

// RTP 固定头最小长度
static constexpr size_t kRtpMinLen = 12;

SfuRoom::SfuRoom(std::string room_id, SendFn send_fn)
    : room_id_(std::move(room_id))
    , send_fn_(std::move(send_fn))
{}

void SfuRoom::add_peer(int peer_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (peer_ssrc_map_.find(peer_id) != peer_ssrc_map_.end())
        return;  // already registered
    peer_ssrc_map_[peer_id] = next_ssrc_++;
    peers_.push_back(peer_id);
}

void SfuRoom::remove_peer(int peer_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    peer_ssrc_map_.erase(peer_id);
    peers_.erase(std::remove(peers_.begin(), peers_.end(), peer_id), peers_.end());
}

void SfuRoom::route_rtp(int from_peer_id, const uint8_t* rtp_data, size_t len) {
    if (len < kRtpMinLen) return;

    std::lock_guard<std::mutex> lk(mutex_);

    auto it = peer_ssrc_map_.find(from_peer_id);
    if (it == peer_ssrc_map_.end()) return;

    uint32_t out_ssrc = it->second;

    // 复制包并重写 SSRC（bytes 8-11，big-endian）
    std::vector<uint8_t> buf(rtp_data, rtp_data + len);
    buf[8]  = (out_ssrc >> 24) & 0xFF;
    buf[9]  = (out_ssrc >> 16) & 0xFF;
    buf[10] = (out_ssrc >>  8) & 0xFF;
    buf[11] =  out_ssrc        & 0xFF;

    // 转发给除 from_peer_id 外的所有 peer
    for (int peer : peers_) {
        if (peer == from_peer_id) continue;
        send_fn_(peer, buf.data(), buf.size());
    }
}

void SfuRoom::route_rtcp(int from_peer_id, const uint8_t* rtcp_data, size_t len) {
    if (len < 4) return;

    std::lock_guard<std::mutex> lk(mutex_);

    // RTCP PT: SR=200, 直接转发给除发送者外的所有 peer
    // 简化处理：将所有 RTCP 包广播出去（生产环境需要解析 SSRC 映射）
    for (int peer : peers_) {
        if (peer == from_peer_id) continue;
        send_fn_(peer, rtcp_data, len);
    }
}
