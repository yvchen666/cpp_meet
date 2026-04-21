#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// SDP 简化子集（只实现会议所需字段）
struct SdpMedia {
    std::string type;         // "audio" / "video"
    uint16_t    port{9};
    std::string proto;        // "UDP/TLS/RTP/SAVPF"
    std::vector<int> fmts;   // payload types

    // a= 属性
    std::string fingerprint;  // "sha-256 AA:BB:..."
    std::string ice_ufrag;
    std::string ice_pwd;
    std::string candidate;    // 第一个 host 候选
    std::vector<std::pair<int,std::string>> rtpmap;  // pt -> "codec/clock"
};

struct Sdp {
    std::string version{"0"};
    std::string origin;
    std::string session_name{"-"};
    std::vector<SdpMedia> media;

    // 序列化为 SDP 字符串
    std::string serialize() const;

    // 从字符串解析
    static std::optional<Sdp> parse(const std::string& text);
};
