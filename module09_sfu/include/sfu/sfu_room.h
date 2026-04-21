#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <string>

// SFU 房间：管理 publisher 和 subscriber
// 接收 publisher 的解密后明文 RTP，重新加密发给每个 subscriber
class SfuRoom {
public:
    using SendFn = std::function<void(int peer_id, const uint8_t* data, size_t len)>;

    SfuRoom(std::string room_id, SendFn send_fn);

    // 注册 publisher（同时也是 subscriber）
    void add_peer(int peer_id);
    void remove_peer(int peer_id);

    // 处理收到的明文 RTP（来自 publisher peer_id）
    // SFU 将其分发给房间内其他所有 peer
    // SSRC 重写：用 {pub_peer_id} 为 key 维护映射
    void route_rtp(int from_peer_id, const uint8_t* rtp_data, size_t len);

    // 处理 RTCP（转发 SR，将接收报告转换 SSRC 后发给 publisher）
    void route_rtcp(int from_peer_id, const uint8_t* rtcp_data, size_t len);

    const std::string& room_id() const { return room_id_; }

private:
    std::string room_id_;
    SendFn send_fn_;
    std::mutex mutex_;
    std::unordered_map<int, uint32_t> peer_ssrc_map_;  // peer_id -> 分配的 out_ssrc
    std::vector<int> peers_;
    uint32_t next_ssrc_{0x10000000};
};
