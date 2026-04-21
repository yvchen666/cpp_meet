#include "sig/room_manager.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

// -----------------------------------------------------------------------
// join
// -----------------------------------------------------------------------

std::string RoomManager::join(int conn_id, const std::string& room_id) {
    std::lock_guard<std::mutex> lk(mutex_);

    // 如果已在某房间，先离开（不加锁版本，内联逻辑）
    auto it = conn_room_.find(conn_id);
    if (it != conn_room_.end()) {
        const std::string& old_room = it->second;
        auto& members = rooms_[old_room];
        members.erase(conn_id);
        if (members.empty()) rooms_.erase(old_room);
        conn_room_.erase(it);
    }

    // 加入新房间
    rooms_[room_id].insert(conn_id);
    conn_room_[conn_id] = room_id;

    // 收集现有成员
    json peers = json::array();
    for (int peer : rooms_[room_id]) {
        if (peer != conn_id)
            peers.push_back(peer);
    }

    // 通知其他成员有新人加入
    if (notify_) {
        json notify_msg = {
            {"type",    "peer_joined"},
            {"peer_id", conn_id},
            {"room_id", room_id}
        };
        std::string notify_str = notify_msg.dump();
        for (int peer : rooms_[room_id]) {
            if (peer != conn_id)
                notify_(peer, notify_str);
        }
    }

    // 返回 room_info JSON
    json reply = {
        {"type",    "room_info"},
        {"room_id", room_id},
        {"peers",   peers}
    };
    return reply.dump();
}

// -----------------------------------------------------------------------
// leave
// -----------------------------------------------------------------------

void RoomManager::leave(int conn_id) {
    std::lock_guard<std::mutex> lk(mutex_);

    auto it = conn_room_.find(conn_id);
    if (it == conn_room_.end()) return;

    const std::string room_id = it->second;
    conn_room_.erase(it);

    auto& members = rooms_[room_id];
    members.erase(conn_id);

    // 通知剩余成员
    if (notify_) {
        json notify_msg = {
            {"type",    "peer_left"},
            {"peer_id", conn_id},
            {"room_id", room_id}
        };
        std::string notify_str = notify_msg.dump();
        for (int peer : members)
            notify_(peer, notify_str);
    }

    if (members.empty())
        rooms_.erase(room_id);
}

// -----------------------------------------------------------------------
// relay
// -----------------------------------------------------------------------

void RoomManager::relay(int from, const std::string& msg_json) {
    try {
        json msg = json::parse(msg_json);

        if (msg.contains("to") && !msg["to"].is_null()) {
            // 点对点转发
            int to = msg["to"].get<int>();
            // 注入 from 字段
            msg["from"] = from;
            if (notify_) notify_(to, msg.dump());
        } else {
            // 广播给房间内其他成员
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = conn_room_.find(from);
            if (it == conn_room_.end()) return;
            const std::string& room_id = it->second;
            msg["from"] = from;
            std::string out = msg.dump();
            for (int peer : rooms_[room_id]) {
                if (peer != from && notify_)
                    notify_(peer, out);
            }
        }
    } catch (...) {
        // 忽略非法 JSON
    }
}
