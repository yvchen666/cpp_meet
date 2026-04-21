#pragma once
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <vector>
#include <functional>
#include <string>

// DTLS 传输层（基于 OpenSSL Memory BIO）
// 完全解耦于 socket：调用方负责收发 UDP
class DtlsTransport {
public:
    // role: true = server, false = client
    explicit DtlsTransport(bool is_server);
    ~DtlsTransport();

    // 不可复制
    DtlsTransport(const DtlsTransport&) = delete;
    DtlsTransport& operator=(const DtlsTransport&) = delete;

    // 初始化（创建 SSL_CTX，加载自签名证书）
    bool init();

    // 喂入从 UDP 收到的数据
    void feed_data(const uint8_t* data, size_t len);

    // 驱动握手（在 epoll 超时时调用）
    void drive_handshake();

    // 驱动 DTLS 超时（DTLSv1_handle_timeout）
    void drive_timeout();

    // 取出需要发送的数据（已握手前为握手包，握手后为加密数据）
    std::vector<uint8_t> pending_send();

    // 握手完成后写入明文，取出密文
    std::vector<uint8_t> encrypt(const uint8_t* plain, size_t len);

    // 握手完成后读取解密的明文
    std::vector<uint8_t> decrypt();

    // 握手是否完成
    bool handshake_done() const;

    // 导出 SRTP 密钥材料（握手完成后调用）
    // keying_material: 60字节 (client_key 16 + server_key 16 + client_salt 14 + server_salt 14)
    bool export_srtp_keying_material(uint8_t out[60]);

    // 证书指纹（SHA-256）
    std::string local_fingerprint() const;

    // 判断是否 DTLS 包（首字节 20-63）
    static bool is_dtls(const uint8_t* data, size_t len);

private:
    bool is_server_;
    SSL_CTX* ctx_{nullptr};
    SSL*     ssl_{nullptr};
    BIO*     rbio_{nullptr};  // 读 BIO（接收 UDP 数据）
    BIO*     wbio_{nullptr};  // 写 BIO（发送 UDP 数据）
    bool     handshake_done_{false};

    // 生成自签名证书和私钥
    static bool generate_self_signed_cert(SSL_CTX* ctx);
};
