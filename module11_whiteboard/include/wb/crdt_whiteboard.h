#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct Point { float x, y; };

struct Stroke {
    std::string uuid;
    std::vector<Point> points;
    uint32_t color{0xFF000000};
    float    width{2.0f};
};

// CRDT LWW-Element-Set（Last-Write-Wins）
// added:   uuid -> (Stroke, timestamp, peer_id)
// removed: uuid -> (timestamp, peer_id)
// 规则：timestamp 最大者胜；同 timestamp 时 peer_id 字典序大者胜
class CrdtWhiteboard {
public:
    explicit CrdtWhiteboard(std::string peer_id);

    // 本地操作
    void local_add(const Stroke& s);
    void local_remove(const std::string& uuid);

    // 接收远端操作
    void remote_add(const Stroke& s, uint64_t ts, const std::string& peer_id);
    void remote_remove(const std::string& uuid, uint64_t ts, const std::string& peer_id);

    // 合并另一个节点的完整状态
    void merge(const CrdtWhiteboard& other);

    // 当前可见笔划
    std::vector<Stroke> visible_strokes() const;

    // 序列化整个状态（用于网络同步）
    std::string serialize_json() const;
    bool parse_json(const std::string& json);

    uint64_t lamport_ts() const { return lamport_ts_; }

private:
    std::string peer_id_;
    uint64_t lamport_ts_{0};

    struct AddEntry    { Stroke stroke; uint64_t ts; std::string peer_id; };
    struct RemoveEntry { uint64_t ts;   std::string peer_id; };

    std::unordered_map<std::string, AddEntry>    added_;
    std::unordered_map<std::string, RemoveEntry> removed_;

    uint64_t tick() { return ++lamport_ts_; }

    uint64_t update_clock(uint64_t remote_ts) {
        if (remote_ts >= lamport_ts_)
            lamport_ts_ = remote_ts + 1;
        return lamport_ts_;
    }

    // 判断 uuid 是否可见
    bool is_visible(const std::string& uuid) const;

    // LWW 胜者：(ts_a, pid_a) vs (ts_b, pid_b)，返回 true 表示 a 胜
    static bool lww_wins(uint64_t ts_a, const std::string& pid_a,
                         uint64_t ts_b, const std::string& pid_b) {
        if (ts_a != ts_b) return ts_a > ts_b;
        return pid_a > pid_b;
    }
};
