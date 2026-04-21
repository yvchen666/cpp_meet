#include <gtest/gtest.h>
#include "wb/crdt_whiteboard.h"

static Stroke make_stroke(const std::string& uuid, float x = 0.f, float y = 0.f) {
    Stroke s;
    s.uuid = uuid;
    s.points = {{x, y}};
    s.color = 0xFF000000;
    s.width = 2.0f;
    return s;
}

TEST(CrdtWhiteboard, LocalAddVisible) {
    CrdtWhiteboard wb("A");
    wb.local_add(make_stroke("s1", 1.f, 2.f));
    auto v = wb.visible_strokes();
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].uuid, "s1");
}

TEST(CrdtWhiteboard, LocalRemoveHidden) {
    CrdtWhiteboard wb("A");
    wb.local_add(make_stroke("s1"));
    wb.local_remove("s1");
    EXPECT_TRUE(wb.visible_strokes().empty());
}

TEST(CrdtWhiteboard, ConcurrentAddConverge) {
    // 两个节点同时添加不同 stroke，merge 后两个都可见
    CrdtWhiteboard wbA("A");
    CrdtWhiteboard wbB("B");

    wbA.local_add(make_stroke("sA"));
    wbB.local_add(make_stroke("sB"));

    wbA.merge(wbB);
    wbB.merge(wbA);

    auto vA = wbA.visible_strokes();
    auto vB = wbB.visible_strokes();
    EXPECT_EQ(vA.size(), 2u);
    EXPECT_EQ(vB.size(), 2u);
}

TEST(CrdtWhiteboard, ConcurrentRemoveWins) {
    // 节点A添加，然后节点B用更大的 ts 删除，merge 后 A 的 add 被覆盖
    CrdtWhiteboard wbA("A");
    CrdtWhiteboard wbB("B");

    wbA.local_add(make_stroke("sX"));
    uint64_t add_ts = wbA.lamport_ts();

    // B 的 remove 用比 A 的 add_ts 更大的 ts
    uint64_t remove_ts = add_ts + 10;
    wbB.remote_remove("sX", remove_ts, "B");

    // A 合并 B 的状态
    wbA.merge(wbB);
    EXPECT_TRUE(wbA.visible_strokes().empty());
}

TEST(CrdtWhiteboard, SerializeRoundtrip) {
    CrdtWhiteboard wb("A");
    Stroke s = make_stroke("s1", 3.f, 4.f);
    s.color = 0xFF112233;
    s.width = 5.0f;
    wb.local_add(s);

    std::string json_str = wb.serialize_json();

    CrdtWhiteboard wb2("X");
    ASSERT_TRUE(wb2.parse_json(json_str));

    auto v = wb2.visible_strokes();
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].uuid, "s1");
    EXPECT_FLOAT_EQ(v[0].points[0].x, 3.f);
    EXPECT_FLOAT_EQ(v[0].points[0].y, 4.f);
    EXPECT_EQ(v[0].color, 0xFF112233u);
    EXPECT_FLOAT_EQ(v[0].width, 5.0f);
}
