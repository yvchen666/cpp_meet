#pragma once
#include <cstdint>
#include <vector>
#include <openssl/aes.h>
#include <openssl/hmac.h>

// 极简 SRTP 实现（AES-CM-128-HMAC-SHA1-80）
// 注意：生产环境请使用 libsrtp2
class SrtpSession {
public:
    // 用导出的密钥初始化（is_send: true=加密发送, false=解密接收）
    // key: 16字节 master key, salt: 14字节 master salt
    bool init(const uint8_t* key, const uint8_t* salt, bool is_send);

    // 保护 RTP 包（加密+认证）
    // in_out: 输入明文RTP，输出 SRTP（追加10字节认证标签）
    bool protect(std::vector<uint8_t>& in_out);

    // 解除保护（验证+解密）
    bool unprotect(std::vector<uint8_t>& in_out);

private:
    uint8_t master_key_[16];
    uint8_t master_salt_[14];
    bool    is_send_;
    uint32_t roc_{0};  // Rollover Counter

    // AES-CM 派生密钥流并异或
    void aes_cm_xor(uint8_t* buf, size_t len,
                    uint32_t ssrc, uint32_t index);

    // HMAC-SHA1-80（截断为10字节）
    void hmac_sha1_80(const uint8_t* data, size_t len,
                      const uint8_t* key, size_t key_len,
                      uint8_t out[10]);
};
