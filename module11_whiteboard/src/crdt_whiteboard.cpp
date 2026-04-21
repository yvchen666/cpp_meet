#include "wb/crdt_whiteboard.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

CrdtWhiteboard::CrdtWhiteboard(std::string peer_id)
    : peer_id_(std::move(peer_id))
{}

void CrdtWhiteboard::local_add(const Stroke& s) {
    uint64_t ts = tick();
    // LWW: 若已存在则只在当前 ts 胜出时替换
    auto it = added_.find(s.uuid);
    if (it == added_.end() || lww_wins(ts, peer_id_, it->second.ts, it->second.peer_id)) {
        added_[s.uuid] = {s, ts, peer_id_};
    }
}

void CrdtWhiteboard::local_remove(const std::string& uuid) {
    uint64_t ts = tick();
    auto it = removed_.find(uuid);
    if (it == removed_.end() || lww_wins(ts, peer_id_, it->second.ts, it->second.peer_id)) {
        removed_[uuid] = {ts, peer_id_};
    }
}

void CrdtWhiteboard::remote_add(const Stroke& s, uint64_t ts, const std::string& peer_id) {
    update_clock(ts);
    auto it = added_.find(s.uuid);
    if (it == added_.end() || lww_wins(ts, peer_id, it->second.ts, it->second.peer_id)) {
        added_[s.uuid] = {s, ts, peer_id};
    }
}

void CrdtWhiteboard::remote_remove(const std::string& uuid, uint64_t ts, const std::string& peer_id) {
    update_clock(ts);
    auto it = removed_.find(uuid);
    if (it == removed_.end() || lww_wins(ts, peer_id, it->second.ts, it->second.peer_id)) {
        removed_[uuid] = {ts, peer_id};
    }
}

void CrdtWhiteboard::merge(const CrdtWhiteboard& other) {
    for (auto& [uuid, entry] : other.added_) {
        remote_add(entry.stroke, entry.ts, entry.peer_id);
    }
    for (auto& [uuid, entry] : other.removed_) {
        remote_remove(uuid, entry.ts, entry.peer_id);
    }
}

bool CrdtWhiteboard::is_visible(const std::string& uuid) const {
    auto add_it = added_.find(uuid);
    if (add_it == added_.end()) return false;

    auto rm_it = removed_.find(uuid);
    if (rm_it == removed_.end()) return true;  // no remove entry

    // visible if add wins over remove
    return lww_wins(add_it->second.ts, add_it->second.peer_id,
                    rm_it->second.ts,  rm_it->second.peer_id);
}

std::vector<Stroke> CrdtWhiteboard::visible_strokes() const {
    std::vector<Stroke> result;
    for (auto& [uuid, entry] : added_) {
        if (is_visible(uuid)) {
            result.push_back(entry.stroke);
        }
    }
    return result;
}

std::string CrdtWhiteboard::serialize_json() const {
    json j;
    j["peer_id"] = peer_id_;
    j["lamport_ts"] = lamport_ts_;

    json added_arr = json::array();
    for (auto& [uuid, entry] : added_) {
        json e;
        e["uuid"]    = entry.stroke.uuid;
        e["color"]   = entry.stroke.color;
        e["width"]   = entry.stroke.width;
        e["ts"]      = entry.ts;
        e["peer_id"] = entry.peer_id;
        json pts = json::array();
        for (auto& p : entry.stroke.points) {
            pts.push_back({{"x", p.x}, {"y", p.y}});
        }
        e["points"] = pts;
        added_arr.push_back(e);
    }
    j["added"] = added_arr;

    json removed_arr = json::array();
    for (auto& [uuid, entry] : removed_) {
        json e;
        e["uuid"]    = uuid;
        e["ts"]      = entry.ts;
        e["peer_id"] = entry.peer_id;
        removed_arr.push_back(e);
    }
    j["removed"] = removed_arr;

    return j.dump();
}

bool CrdtWhiteboard::parse_json(const std::string& s) {
    try {
        auto j = json::parse(s);
        peer_id_    = j.at("peer_id").get<std::string>();
        lamport_ts_ = j.at("lamport_ts").get<uint64_t>();

        added_.clear();
        for (auto& e : j.at("added")) {
            Stroke stroke;
            stroke.uuid  = e.at("uuid").get<std::string>();
            stroke.color = e.at("color").get<uint32_t>();
            stroke.width = e.at("width").get<float>();
            for (auto& p : e.at("points")) {
                stroke.points.push_back({p.at("x").get<float>(), p.at("y").get<float>()});
            }
            AddEntry entry{stroke, e.at("ts").get<uint64_t>(), e.at("peer_id").get<std::string>()};
            added_[stroke.uuid] = entry;
        }

        removed_.clear();
        for (auto& e : j.at("removed")) {
            std::string uuid = e.at("uuid").get<std::string>();
            RemoveEntry entry{e.at("ts").get<uint64_t>(), e.at("peer_id").get<std::string>()};
            removed_[uuid] = entry;
        }
        return true;
    } catch (...) {
        return false;
    }
}
