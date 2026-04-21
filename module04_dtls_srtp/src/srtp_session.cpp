#include "dtls/srtp_session.h"
#include <cstring>
#include <openssl/evp.h>
#include <openssl/aes.h>

// ─── AES-CM 实现 ──────────────────────────────────────────────────────────────
// RFC 3711: IV = (k_s * 2^16) XOR (SSRC * 2^64) XOR (index * 2^16)
// 用128位大端整数表示 IV

static void build_aes_cm_iv(uint8_t iv[16], const uint8_t* salt14,
                              uint32_t ssrc, uint64_t index)
{
    // Per RFC 3711 §4.3.1: IV = session_salt XOR (SSRC << 64) XOR (pkt_index << 16)
    // In big-endian 16-byte layout:
    //   bytes[0..3]  : 0  (bits 127..96)
    //   bytes[4..7]  : SSRC
    //   bytes[8..13] : packet index (48 bits)
    //   bytes[14..15]: 0
    // We use master_salt directly (no full KDF for this minimal impl)
    memset(iv, 0, 16);
    memcpy(iv, salt14, 14);

    // XOR SSRC at bytes [4..7]
    iv[4]  ^= (ssrc >> 24) & 0xFF;
    iv[5]  ^= (ssrc >> 16) & 0xFF;
    iv[6]  ^= (ssrc >> 8)  & 0xFF;
    iv[7]  ^= (ssrc)       & 0xFF;

    // XOR packet index at bytes [8..13] (48 bits, index is 48-bit)
    iv[8]  ^= (index >> 40) & 0xFF;
    iv[9]  ^= (index >> 32) & 0xFF;
    iv[10] ^= (index >> 24) & 0xFF;
    iv[11] ^= (index >> 16) & 0xFF;
    iv[12] ^= (index >> 8)  & 0xFF;
    iv[13] ^= (index)       & 0xFF;
}

void SrtpSession::aes_cm_xor(uint8_t* buf, size_t len,
                               uint32_t ssrc, uint32_t index)
{
    uint8_t iv[16];
    build_aes_cm_iv(iv, master_salt_, ssrc, index);

    AES_KEY aes_key;
    AES_set_encrypt_key(master_key_, 128, &aes_key);

    // AES-CM: encrypt successive counter blocks and XOR with data
    uint8_t counter[16];
    memcpy(counter, iv, 16);

    size_t pos = 0;
    while (pos < len) {
        uint8_t keystream[16];
        AES_encrypt(counter, keystream, &aes_key);

        size_t chunk = (len - pos < 16) ? (len - pos) : 16;
        for (size_t i = 0; i < chunk; ++i) {
            buf[pos + i] ^= keystream[i];
        }
        pos += chunk;

        // Increment counter (big-endian, last 2 bytes)
        for (int i = 15; i >= 0; --i) {
            if (++counter[i] != 0) break;
        }
    }
}

void SrtpSession::hmac_sha1_80(const uint8_t* data, size_t len,
                                 const uint8_t* key, size_t key_len,
                                 uint8_t out[10])
{
    uint8_t digest[20];
    unsigned int dlen = 20;
    HMAC(EVP_sha1(), key, static_cast<int>(key_len),
         data, len, digest, &dlen);
    memcpy(out, digest, 10);
}

// ─── SrtpSession ─────────────────────────────────────────────────────────────

bool SrtpSession::init(const uint8_t* key, const uint8_t* salt, bool is_send) {
    memcpy(master_key_,  key,  16);
    memcpy(master_salt_, salt, 14);
    is_send_ = is_send;
    roc_ = 0;
    return true;
}

// RTP header is at least 12 bytes:
//  0      : V(2) P(1) X(1) CC(4)
//  1      : M(1) PT(7)
//  2-3    : Sequence Number
//  4-7    : Timestamp
//  8-11   : SSRC

bool SrtpSession::protect(std::vector<uint8_t>& in_out) {
    if (in_out.size() < 12) return false;

    uint16_t seq  = (uint16_t(in_out[2]) << 8) | in_out[3];
    uint32_t ssrc = (uint32_t(in_out[8])  << 24) | (uint32_t(in_out[9]) << 16) |
                    (uint32_t(in_out[10]) << 8)  | in_out[11];

    // Packet index = ROC * 2^16 + SEQ
    uint64_t pkt_index = (uint64_t(roc_) << 16) | seq;

    // Encrypt payload (bytes 12 onwards)
    aes_cm_xor(in_out.data() + 12, in_out.size() - 12, ssrc, pkt_index);

    // Append HMAC-SHA1-80 over encrypted packet
    uint8_t tag[10];
    hmac_sha1_80(in_out.data(), in_out.size(), master_key_, 16, tag);
    in_out.insert(in_out.end(), tag, tag + 10);

    return true;
}

bool SrtpSession::unprotect(std::vector<uint8_t>& in_out) {
    if (in_out.size() < 12 + 10) return false;

    size_t payload_end = in_out.size() - 10;

    // Verify HMAC-SHA1-80
    uint8_t expected_tag[10];
    hmac_sha1_80(in_out.data(), payload_end, master_key_, 16, expected_tag);

    // Constant-time comparison
    uint8_t diff = 0;
    for (int i = 0; i < 10; ++i) {
        diff |= (expected_tag[i] ^ in_out[payload_end + i]);
    }
    if (diff != 0) return false;

    // Remove tag
    in_out.resize(payload_end);

    uint16_t seq  = (uint16_t(in_out[2]) << 8) | in_out[3];
    uint32_t ssrc = (uint32_t(in_out[8])  << 24) | (uint32_t(in_out[9]) << 16) |
                    (uint32_t(in_out[10]) << 8)  | in_out[11];
    uint64_t pkt_index = (uint64_t(roc_) << 16) | seq;

    // Decrypt payload
    aes_cm_xor(in_out.data() + 12, in_out.size() - 12, ssrc, pkt_index);

    return true;
}
