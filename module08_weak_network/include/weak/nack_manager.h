#pragma once
#include <rtp/rtcp_packet.h>
#include <set>
#include <map>
#include <vector>
#include <chrono>

// NACK 管理器（接收端）
class NackManager {
public:
    // 报告新收到的序列号
    void on_receive(uint16_t seq);

    // 返回需要发送的 NACK 列表（100ms 内未收到的间隙序列号）
    // 需要定期调用
    std::vector<uint16_t> get_nack_list(std::chrono::steady_clock::time_point now);

private:
    std::set<uint16_t> received_;
    uint16_t max_seen_{0};
    bool started_{false};
    std::map<uint16_t, std::chrono::steady_clock::time_point> missing_;
};
