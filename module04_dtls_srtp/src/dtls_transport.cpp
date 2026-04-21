#include "dtls/dtls_transport.h"
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>
#include <sstream>
#include <iomanip>

DtlsTransport::DtlsTransport(bool is_server) : is_server_(is_server) {}

DtlsTransport::~DtlsTransport() {
    if (ssl_) { SSL_free(ssl_); ssl_ = nullptr; }
    if (ctx_) { SSL_CTX_free(ctx_); ctx_ = nullptr; }
    // BIO 由 SSL 管理，不需要单独释放
}

bool DtlsTransport::init() {
    // 创建 DTLS SSL_CTX
    const SSL_METHOD* method = is_server_ ? DTLS_server_method() : DTLS_client_method();
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) return false;

    // 生成自签名证书
    if (!generate_self_signed_cert(ctx_)) {
        SSL_CTX_free(ctx_); ctx_ = nullptr;
        return false;
    }

    // 设置加密套件
    SSL_CTX_set_cipher_list(ctx_, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");

    // 不验证对端证书（测试用）
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);

    // SRTP 扩展
    SSL_CTX_set_tlsext_use_srtp(ctx_, "SRTP_AES128_CM_SHA1_80");

    // 创建 SSL 对象
    ssl_ = SSL_new(ctx_);
    if (!ssl_) return false;

    // 创建 Memory BIO
    rbio_ = BIO_new(BIO_s_mem());
    wbio_ = BIO_new(BIO_s_mem());
    if (!rbio_ || !wbio_) return false;

    BIO_set_mem_eof_return(rbio_, -1);
    BIO_set_mem_eof_return(wbio_, -1);

    SSL_set_bio(ssl_, rbio_, wbio_);

    if (is_server_) {
        SSL_set_accept_state(ssl_);
    } else {
        SSL_set_connect_state(ssl_);
    }

    return true;
}

bool DtlsTransport::generate_self_signed_cert(SSL_CTX* ctx) {
    // 生成 RSA 密钥
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) return false;

    RSA* rsa = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);
    if (!rsa) { EVP_PKEY_free(pkey); return false; }

    EVP_PKEY_assign_RSA(pkey, rsa);

    // 创建 X509 证书
    X509* x509 = X509_new();
    if (!x509) { EVP_PKEY_free(pkey); return false; }

    // 设置序列号
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    // 有效期：当前时间 ± 1年
    X509_gmtime_adj(X509_get_notBefore(x509), -365 * 24 * 3600);
    X509_gmtime_adj(X509_get_notAfter(x509),   365 * 24 * 3600);

    // 设置公钥
    X509_set_pubkey(x509, pkey);

    // 设置主题
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                (unsigned char*)"dtls_test", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    // 自签名
    X509_sign(x509, pkey, EVP_sha256());

    // 载入到 SSL_CTX
    SSL_CTX_use_certificate(ctx, x509);
    SSL_CTX_use_PrivateKey(ctx, pkey);

    X509_free(x509);
    EVP_PKEY_free(pkey);
    return true;
}

void DtlsTransport::feed_data(const uint8_t* data, size_t len) {
    BIO_write(rbio_, data, static_cast<int>(len));
}

void DtlsTransport::drive_handshake() {
    if (handshake_done_) return;

    int ret = SSL_do_handshake(ssl_);
    if (ret == 1) {
        handshake_done_ = true;
        return;
    }

    int err = SSL_get_error(ssl_, ret);
    // SSL_ERROR_WANT_READ / WANT_WRITE 是正常的异步等待
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
        // 其他错误，握手失败（生产代码应记录日志）
        (void)err;
    }
}

void DtlsTransport::drive_timeout() {
    if (handshake_done_) return;
    DTLSv1_handle_timeout(ssl_);
}

std::vector<uint8_t> DtlsTransport::pending_send() {
    std::vector<uint8_t> result;
    char buf[4096];
    int n;
    while ((n = BIO_read(wbio_, buf, sizeof(buf))) > 0) {
        result.insert(result.end(), buf, buf + n);
    }
    return result;
}

std::vector<uint8_t> DtlsTransport::encrypt(const uint8_t* plain, size_t len) {
    if (!handshake_done_) return {};
    SSL_write(ssl_, plain, static_cast<int>(len));
    return pending_send();
}

std::vector<uint8_t> DtlsTransport::decrypt() {
    if (!handshake_done_) return {};
    std::vector<uint8_t> result;
    char buf[4096];
    int n;
    while ((n = SSL_read(ssl_, buf, sizeof(buf))) > 0) {
        result.insert(result.end(), buf, buf + n);
    }
    return result;
}

bool DtlsTransport::handshake_done() const {
    return handshake_done_;
}

bool DtlsTransport::export_srtp_keying_material(uint8_t out[60]) {
    if (!handshake_done_) return false;
    // RFC 5764: label = "EXTRACTOR-dtls_srtp"
    int ret = SSL_export_keying_material(ssl_, out, 60,
                                         "EXTRACTOR-dtls_srtp", 19,
                                         nullptr, 0, 0);
    return ret == 1;
}

std::string DtlsTransport::local_fingerprint() const {
    X509* cert = SSL_get_certificate(ssl_);
    if (!cert) return "";

    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    X509_digest(cert, EVP_sha256(), digest, &len);

    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i) {
        if (i > 0) oss << ":";
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
}

bool DtlsTransport::is_dtls(const uint8_t* data, size_t len) {
    if (len < 1) return false;
    return data[0] >= 20 && data[0] <= 63;
}
