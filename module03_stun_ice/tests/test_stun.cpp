#include <gtest/gtest.h>
#include "ice/stun_message.h"
#include <cstring>
#include <arpa/inet.h>

TEST(StunMessage, BuildAndParse) {
    StunMessage msg(StunType::BindingRequest);

    // 设置固定事务ID
    uint8_t tid[12] = {0x01,0x02,0x03,0x04,0x05,0x06,
                       0x07,0x08,0x09,0x0a,0x0b,0x0c};
    msg.set_transaction_id(tid);

    // 添加字符串和数值属性
    msg.add_string_attr(StunAttr::Username, "testuser:peer");
    msg.add_uint32_attr(StunAttr::Priority, 0x6E7F1EFF);

    // 序列化
    auto buf = msg.serialize();
    ASSERT_GE(buf.size(), 20u);

    // 解析
    StunMessage parsed;
    ASSERT_TRUE(parsed.parse(buf.data(), buf.size()));

    // 验证类型和事务ID
    EXPECT_EQ(parsed.type(), StunType::BindingRequest);
    EXPECT_EQ(memcmp(parsed.transaction_id(), tid, 12), 0);

    // 验证属性
    std::string uname;
    ASSERT_TRUE(parsed.get_string_attr(StunAttr::Username, uname));
    EXPECT_EQ(uname, "testuser:peer");

    uint32_t prio;
    ASSERT_TRUE(parsed.get_uint32_attr(StunAttr::Priority, prio));
    EXPECT_EQ(prio, 0x6E7F1EFFu);
}

TEST(StunMessage, XorMappedAddress) {
    StunMessage msg(StunType::BindingResponse);

    // 192.168.1.100:12345
    uint32_t ip_net = htonl(0xC0A80164); // 192.168.1.100
    uint16_t port   = 12345;
    msg.add_xor_mapped_address(ip_net, port);

    auto buf = msg.serialize();
    StunMessage parsed;
    ASSERT_TRUE(parsed.parse(buf.data(), buf.size()));

    uint32_t got_ip;
    uint16_t got_port;
    ASSERT_TRUE(parsed.get_xor_mapped_address(got_ip, got_port));

    EXPECT_EQ(got_ip, ip_net);
    EXPECT_EQ(got_port, port);
}

TEST(StunMessage, IsStun) {
    // 有效 STUN 包（BindingRequest + magic cookie）
    StunMessage msg(StunType::BindingRequest);
    auto buf = msg.serialize();
    EXPECT_TRUE(StunMessage::is_stun(buf.data(), buf.size()));

    // 太短
    EXPECT_FALSE(StunMessage::is_stun(buf.data(), 10));

    // 首字节高位非零
    uint8_t bad[20] = {};
    bad[0] = 0x80; // 高位置 1
    bad[4] = 0x21; bad[5] = 0x12; bad[6] = 0xA4; bad[7] = 0x42;
    EXPECT_FALSE(StunMessage::is_stun(bad, 20));

    // magic cookie 错误
    uint8_t bad2[20] = {};
    bad2[4] = 0x00; bad2[5] = 0x00; bad2[6] = 0x00; bad2[7] = 0x00;
    EXPECT_FALSE(StunMessage::is_stun(bad2, 20));
}

TEST(StunMessage, MessageIntegrity) {
    StunMessage msg(StunType::BindingRequest);
    uint8_t tid[12] = {};
    msg.set_transaction_id(tid);
    msg.add_string_attr(StunAttr::Username, "user:peer");
    msg.add_message_integrity("secret_password");
    msg.add_fingerprint();

    auto buf = msg.serialize();
    ASSERT_GE(buf.size(), 20u);

    // 可以解析
    StunMessage parsed;
    ASSERT_TRUE(parsed.parse(buf.data(), buf.size()));
    EXPECT_EQ(parsed.type(), StunType::BindingRequest);
}
