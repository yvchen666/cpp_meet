#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

// STUN 消息类型（RFC 5389）
enum class StunType : uint16_t {
    BindingRequest  = 0x0001,
    BindingResponse = 0x0101,
    BindingError    = 0x0111,
};

// STUN 属性类型
enum class StunAttr : uint16_t {
    MappedAddress    = 0x0001,
    Username         = 0x0006,
    MessageIntegrity = 0x0008,
    ErrorCode        = 0x0009,
    XorMappedAddress = 0x0020,
    Priority         = 0x0024,
    UseCandidate     = 0x0025,
    IceControlled    = 0x8029,
    IceControlling   = 0x802A,
    Fingerprint      = 0x8028,
};

// STUN Magic Cookie
constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

class StunMessage {
public:
    StunMessage() = default;
    StunMessage(StunType type);

    // 设置事务ID（12字节）
    void set_transaction_id(const uint8_t* tid);
    const uint8_t* transaction_id() const;

    // 添加属性
    void add_string_attr(StunAttr attr, const std::string& value);
    void add_uint32_attr(StunAttr attr, uint32_t value);
    void add_xor_mapped_address(uint32_t ip, uint16_t port);

    // 添加 MESSAGE-INTEGRITY（HMAC-SHA1，需要 password）
    void add_message_integrity(const std::string& password);

    // 添加 FINGERPRINT (CRC32 xor 0x5354554e)
    void add_fingerprint();

    // 序列化成字节
    std::vector<uint8_t> serialize() const;

    // 解析，返回 false 失败
    bool parse(const uint8_t* data, size_t len);

    StunType type() const { return type_; }

    // 获取属性值
    bool get_string_attr(StunAttr attr, std::string& out) const;
    bool get_uint32_attr(StunAttr attr, uint32_t& out) const;
    bool get_xor_mapped_address(uint32_t& ip, uint16_t& port) const;

    // 检测是否是 STUN 包（首字节 0-3）
    static bool is_stun(const uint8_t* data, size_t len);

private:
    StunType type_{StunType::BindingRequest};
    uint8_t  transaction_id_[12]{};
    // attr_type -> raw value bytes（不含类型和长度）
    std::vector<std::pair<StunAttr, std::vector<uint8_t>>> attrs_;

    // CRC32 for FINGERPRINT
    static uint32_t crc32(const uint8_t* data, size_t len);
};
