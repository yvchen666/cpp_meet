#pragma once
#include "stun_message.h"
#include <string>
#include <functional>
#include <netinet/in.h>

enum class IceRole { Controlling, Controlled };

struct IceCandidate {
    std::string foundation;
    uint32_t    priority;
    uint32_t    ip;    // 网络字节序
    uint16_t    port;  // 主机字节序
    std::string type;  // "host" / "srflx"
};

// ICE Agent（简化版，只支持 host 候选）
class IceAgent {
public:
    IceAgent(IceRole role, const std::string& local_ufrag,
             const std::string& local_pwd);

    void set_remote_credentials(const std::string& ufrag, const std::string& pwd);
    void add_remote_candidate(const IceCandidate& c);

    // 接收 UDP 包，如果是 STUN 则处理并生成响应
    // on_send 回调用于发送数据
    using SendFn = std::function<void(const uint8_t*, size_t, const sockaddr_in&)>;
    void on_packet(const uint8_t* data, size_t len,
                   const sockaddr_in& from, SendFn on_send);

    // 发送 binding request 到对端（Connectivity Check）
    void send_binding_request(const sockaddr_in& to, SendFn on_send);

    bool is_connected() const { return connected_; }

    const std::string& local_ufrag() const { return local_ufrag_; }
    const std::string& local_pwd()   const { return local_pwd_; }

private:
    IceRole role_;
    std::string local_ufrag_, local_pwd_;
    std::string remote_ufrag_, remote_pwd_;
    std::vector<IceCandidate> remote_candidates_;
    bool connected_{false};
};
