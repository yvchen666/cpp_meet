#include "ice/ice_agent.h"
#include <cstring>
#include <arpa/inet.h>
#include <cstdlib>

IceAgent::IceAgent(IceRole role, const std::string& local_ufrag,
                   const std::string& local_pwd)
    : role_(role), local_ufrag_(local_ufrag), local_pwd_(local_pwd)
{}

void IceAgent::set_remote_credentials(const std::string& ufrag, const std::string& pwd) {
    remote_ufrag_ = ufrag;
    remote_pwd_   = pwd;
}

void IceAgent::add_remote_candidate(const IceCandidate& c) {
    remote_candidates_.push_back(c);
}

void IceAgent::on_packet(const uint8_t* data, size_t len,
                         const sockaddr_in& from, SendFn on_send)
{
    if (!StunMessage::is_stun(data, len)) return;

    StunMessage req;
    if (!req.parse(data, len)) return;

    if (req.type() == StunType::BindingRequest) {
        // 验证 USERNAME 属性
        std::string username;
        if (req.get_string_attr(StunAttr::Username, username)) {
            // 期望格式: local_ufrag:remote_ufrag
            std::string expected = local_ufrag_ + ":" + remote_ufrag_;
            if (username != expected) return;
        }

        // 构建 BindingResponse
        StunMessage resp(StunType::BindingResponse);
        resp.set_transaction_id(req.transaction_id());

        // 添加 XOR-MAPPED-ADDRESS
        resp.add_xor_mapped_address(from.sin_addr.s_addr,
                                    ntohs(from.sin_port));

        // 添加 MESSAGE-INTEGRITY（用本地密码）
        resp.add_message_integrity(local_pwd_);

        // 添加 FINGERPRINT
        resp.add_fingerprint();

        auto buf = resp.serialize();
        on_send(buf.data(), buf.size(), from);

        connected_ = true;

    } else if (req.type() == StunType::BindingResponse) {
        // 收到 response，连接建立
        connected_ = true;
    }
}

void IceAgent::send_binding_request(const sockaddr_in& to, SendFn on_send) {
    StunMessage req(StunType::BindingRequest);

    // USERNAME: remote_ufrag:local_ufrag（发送方角度）
    std::string username = remote_ufrag_ + ":" + local_ufrag_;
    req.add_string_attr(StunAttr::Username, username);

    // PRIORITY
    uint32_t priority = (110 << 24) | (65535 << 8) | 255;
    req.add_uint32_attr(StunAttr::Priority, priority);

    // ICE-CONTROLLING or ICE-CONTROLLED（8字节 tiebreaker）
    uint64_t tiebreaker = ((uint64_t)rand() << 32) | rand();
    std::vector<uint8_t> tb_bytes(8);
    for (int i = 0; i < 8; ++i) {
        tb_bytes[i] = (tiebreaker >> (56 - i * 8)) & 0xFF;
    }

    if (role_ == IceRole::Controlling) {
        req.add_string_attr(StunAttr::IceControlling,
                            std::string(tb_bytes.begin(), tb_bytes.end()));
    } else {
        req.add_string_attr(StunAttr::IceControlled,
                            std::string(tb_bytes.begin(), tb_bytes.end()));
    }

    // MESSAGE-INTEGRITY（用对端密码）
    req.add_message_integrity(remote_pwd_);

    // FINGERPRINT
    req.add_fingerprint();

    auto buf = req.serialize();
    on_send(buf.data(), buf.size(), to);
}
