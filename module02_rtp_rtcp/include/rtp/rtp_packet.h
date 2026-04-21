#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

// RTP 包（RFC 3550）
// 存储格式：raw bytes，accessor 直接解析，不使用 #pragma pack 结构体
class RtpPacket {
public:
    RtpPacket() = default;
    explicit RtpPacket(std::vector<uint8_t> data);

    // 从 raw bytes 解析，返回 false 表示格式错误
    bool parse(const uint8_t* data, size_t len);

    // 构建 RTP 包（无扩展，无CC）
    void build(uint8_t payload_type, uint16_t seq, uint32_t timestamp,
               uint32_t ssrc, const uint8_t* payload, size_t payload_len,
               bool marker = false);

    // Getters（直接从 buf_ 读取，大端序）
    uint8_t  version()      const;   // 必须 == 2
    bool     padding()      const;
    bool     extension()    const;
    uint8_t  cc()           const;
    bool     marker()       const;
    uint8_t  payload_type() const;
    uint16_t sequence()     const;
    uint32_t timestamp()    const;
    uint32_t ssrc()         const;

    const uint8_t* payload()      const;
    size_t         payload_size() const;

    const uint8_t* data() const;
    size_t         size() const;

    // RFC 3550 A.1: 序列号比较（wrap-around 安全）
    // 返回 true 表示 a 比 b 更新（靠后）
    static bool seq_gt(uint16_t a, uint16_t b) {
        return (int16_t)(a - b) > 0;
    }

private:
    std::vector<uint8_t> buf_;
    size_t               payload_offset_{0};
};
