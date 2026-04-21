#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <functional>

// 房间管理
class RoomManager {
public:
    using NotifyFn = std::function<void(int conn_id, const std::string& msg)>;

    void set_notify(NotifyFn fn) { notify_ = fn; }

    // conn_id 加入房间，返回房间内其他成员列表（JSON）
    std::string join(int conn_id, const std::string& room_id);

    // conn_id 离开（连接断开时调用）
    void leave(int conn_id);

    // 转发信令消息（offer/answer/ice_candidate）
    void relay(int from, const std::string& msg_json);

private:
    std::mutex mutex_;
    // room_id -> set<conn_id>
    std::unordered_map<std::string, std::unordered_set<int>> rooms_;
    // conn_id -> room_id
    std::unordered_map<int, std::string> conn_room_;
    NotifyFn notify_;
};
