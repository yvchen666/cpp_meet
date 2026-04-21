#include "ice/stun_message.h"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

// ─── CRC32 查表法（运行时生成表，多项式 0xEDB88320） ─────────────────────────

static uint32_t make_crc32_table_entry(uint8_t i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k)
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    return c;
}

uint32_t StunMessage::crc32(const uint8_t* data, size_t len) {
    // 惰性初始化查表
    static uint32_t table[256] = {};
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 256; ++i) table[i] = make_crc32_table_entry(i);
        initialized = true;
    }
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i)
        crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFF];
    return crc ^ 0xFFFFFFFF;
}

// ─── 辅助函数 ─────────────────────────────────────────────────────────────────

static void write_u16(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back((val >> 8) & 0xFF);
    v.push_back(val & 0xFF);
}

static void write_u32(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back((val >> 24) & 0xFF);
    v.push_back((val >> 16) & 0xFF);
    v.push_back((val >> 8) & 0xFF);
    v.push_back(val & 0xFF);
}

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
}

static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | p[3];
}

// ─── StunMessage ─────────────────────────────────────────────────────────────

StunMessage::StunMessage(StunType type) : type_(type) {
    // 生成随机事务ID
    for (int i = 0; i < 12; ++i) {
        transaction_id_[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
}

void StunMessage::set_transaction_id(const uint8_t* tid) {
    memcpy(transaction_id_, tid, 12);
}

const uint8_t* StunMessage::transaction_id() const {
    return transaction_id_;
}

void StunMessage::add_string_attr(StunAttr attr, const std::string& value) {
    std::vector<uint8_t> bytes(value.begin(), value.end());
    attrs_.push_back({attr, std::move(bytes)});
}

void StunMessage::add_uint32_attr(StunAttr attr, uint32_t value) {
    std::vector<uint8_t> bytes(4);
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    attrs_.push_back({attr, std::move(bytes)});
}

void StunMessage::add_xor_mapped_address(uint32_t ip, uint16_t port) {
    // XOR-MAPPED-ADDRESS: 1字节0 + 1字节family(0x01) + 2字节xor port + 4字节xor ip
    std::vector<uint8_t> bytes(8);
    bytes[0] = 0x00;
    bytes[1] = 0x01; // IPv4
    uint16_t xport = port ^ (STUN_MAGIC_COOKIE >> 16);
    uint32_t xip   = ntohl(ip) ^ STUN_MAGIC_COOKIE;
    bytes[2] = (xport >> 8) & 0xFF;
    bytes[3] = xport & 0xFF;
    bytes[4] = (xip >> 24) & 0xFF;
    bytes[5] = (xip >> 16) & 0xFF;
    bytes[6] = (xip >> 8) & 0xFF;
    bytes[7] = xip & 0xFF;
    attrs_.push_back({StunAttr::XorMappedAddress, std::move(bytes)});
}

// 序列化属性列表到 buf（不含20字节头），用于 MI 和 FINGERPRINT 计算
static std::vector<uint8_t> serialize_attrs(
    const std::vector<std::pair<StunAttr, std::vector<uint8_t>>>& attrs)
{
    std::vector<uint8_t> buf;
    for (auto& [type, val] : attrs) {
        write_u16(buf, static_cast<uint16_t>(type));
        write_u16(buf, static_cast<uint16_t>(val.size()));
        buf.insert(buf.end(), val.begin(), val.end());
        // 4字节对齐填充
        size_t pad = (4 - (val.size() % 4)) % 4;
        for (size_t i = 0; i < pad; ++i) buf.push_back(0);
    }
    return buf;
}

std::vector<uint8_t> StunMessage::serialize() const {
    std::vector<uint8_t> attr_bytes = serialize_attrs(attrs_);

    std::vector<uint8_t> out;
    out.reserve(20 + attr_bytes.size());

    write_u16(out, static_cast<uint16_t>(type_));
    write_u16(out, static_cast<uint16_t>(attr_bytes.size()));
    write_u32(out, STUN_MAGIC_COOKIE);
    out.insert(out.end(), transaction_id_, transaction_id_ + 12);
    out.insert(out.end(), attr_bytes.begin(), attr_bytes.end());
    return out;
}

void StunMessage::add_message_integrity(const std::string& password) {
    // RFC 5389: MI 属性中，消息长度字段 = 当前属性长度 + MI属性(24字节)
    // 先构建不含 MI 的属性字节
    std::vector<uint8_t> attr_bytes = serialize_attrs(attrs_);
    size_t msg_len_for_mi = attr_bytes.size() + 24; // 4(type+len) + 20(sha1)

    // 构建用于 HMAC 的消息头
    std::vector<uint8_t> hmac_input;
    hmac_input.reserve(20 + attr_bytes.size());
    write_u16(hmac_input, static_cast<uint16_t>(type_));
    write_u16(hmac_input, static_cast<uint16_t>(msg_len_for_mi));
    write_u32(hmac_input, STUN_MAGIC_COOKIE);
    hmac_input.insert(hmac_input.end(), transaction_id_, transaction_id_ + 12);
    hmac_input.insert(hmac_input.end(), attr_bytes.begin(), attr_bytes.end());

    // HMAC-SHA1
    uint8_t digest[20];
    unsigned int dlen = 20;
    HMAC(EVP_sha1(),
         password.data(), static_cast<int>(password.size()),
         hmac_input.data(), hmac_input.size(),
         digest, &dlen);

    std::vector<uint8_t> mi_val(digest, digest + 20);
    attrs_.push_back({StunAttr::MessageIntegrity, std::move(mi_val)});
}

void StunMessage::add_fingerprint() {
    // FINGERPRINT: CRC32 of 当前序列化消息（含 FINGERPRINT 属性长度占位），xor 0x5354554e
    // 先序列化不含 FINGERPRINT 的消息，但消息长度要包含 FINGERPRINT 属性（8字节：4头+4值）
    std::vector<uint8_t> attr_bytes = serialize_attrs(attrs_);
    size_t msg_len_for_fp = attr_bytes.size() + 8;

    std::vector<uint8_t> fp_input;
    fp_input.reserve(20 + attr_bytes.size());
    write_u16(fp_input, static_cast<uint16_t>(type_));
    write_u16(fp_input, static_cast<uint16_t>(msg_len_for_fp));
    write_u32(fp_input, STUN_MAGIC_COOKIE);
    fp_input.insert(fp_input.end(), transaction_id_, transaction_id_ + 12);
    fp_input.insert(fp_input.end(), attr_bytes.begin(), attr_bytes.end());

    uint32_t fp = crc32(fp_input.data(), fp_input.size()) ^ 0x5354554eu;
    std::vector<uint8_t> fp_val(4);
    fp_val[0] = (fp >> 24) & 0xFF;
    fp_val[1] = (fp >> 16) & 0xFF;
    fp_val[2] = (fp >> 8) & 0xFF;
    fp_val[3] = fp & 0xFF;
    attrs_.push_back({StunAttr::Fingerprint, std::move(fp_val)});
}

bool StunMessage::parse(const uint8_t* data, size_t len) {
    if (len < 20) return false;

    // 验证 magic cookie
    uint32_t cookie = read_u32(data + 4);
    if (cookie != STUN_MAGIC_COOKIE) return false;

    // 前两位必须为 0（STUN 包标志）
    if (data[0] & 0xC0) return false;

    type_ = static_cast<StunType>(read_u16(data));
    uint16_t msg_len = read_u16(data + 2);
    memcpy(transaction_id_, data + 8, 12);

    attrs_.clear();

    if (len < 20 + msg_len) return false;

    size_t offset = 20;
    size_t end = 20 + msg_len;
    while (offset + 4 <= end) {
        uint16_t attr_type = read_u16(data + offset);
        uint16_t attr_len  = read_u16(data + offset + 2);
        offset += 4;

        if (offset + attr_len > end) return false;

        std::vector<uint8_t> val(data + offset, data + offset + attr_len);
        attrs_.push_back({static_cast<StunAttr>(attr_type), std::move(val)});

        // 4字节对齐
        offset += attr_len;
        size_t pad = (4 - (attr_len % 4)) % 4;
        offset += pad;
    }
    return true;
}

bool StunMessage::get_string_attr(StunAttr attr, std::string& out) const {
    for (auto& [type, val] : attrs_) {
        if (type == attr) {
            out = std::string(val.begin(), val.end());
            return true;
        }
    }
    return false;
}

bool StunMessage::get_uint32_attr(StunAttr attr, uint32_t& out) const {
    for (auto& [type, val] : attrs_) {
        if (type == attr && val.size() >= 4) {
            out = read_u32(val.data());
            return true;
        }
    }
    return false;
}

bool StunMessage::get_xor_mapped_address(uint32_t& ip, uint16_t& port) const {
    for (auto& [type, val] : attrs_) {
        if (type == StunAttr::XorMappedAddress && val.size() >= 8) {
            uint16_t xport = read_u16(val.data() + 2);
            uint32_t xip   = read_u32(val.data() + 4);
            port = xport ^ (STUN_MAGIC_COOKIE >> 16);
            ip   = htonl(xip ^ STUN_MAGIC_COOKIE);
            return true;
        }
    }
    return false;
}

bool StunMessage::is_stun(const uint8_t* data, size_t len) {
    if (len < 20) return false;
    // 首字节高两位为 0
    if (data[0] & 0xC0) return false;
    // magic cookie
    uint32_t cookie = read_u32(data + 4);
    return cookie == STUN_MAGIC_COOKIE;
}
