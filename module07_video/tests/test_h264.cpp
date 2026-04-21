#include <gtest/gtest.h>
#include "video/h264_encoder.h"
#include "video/rtp_video_packetizer.h"
#include <cstring>

// 创建一帧黑色 YUV420P
static YuvFrame make_black_frame(int w, int h, uint32_t ts = 0) {
    YuvFrame f;
    f.width  = w;
    f.height = h;
    f.timestamp_ms = ts;
    int y_size = w * h;
    f.data.resize(y_size + y_size / 2, 0);
    // Y=0, U=128, V=128 表示黑色
    std::fill(f.data.begin() + y_size, f.data.end(), 128);
    return f;
}

TEST(H264Encoder, Init) {
    H264Encoder enc;
    EXPECT_TRUE(enc.init(640, 480, 30, 1000));
}

TEST(H264Encoder, EncodeFrame) {
    H264Encoder enc;
    ASSERT_TRUE(enc.init(320, 240, 30, 500));

    auto frame = make_black_frame(320, 240, 0);
    auto nals = enc.encode(frame);
    // 第一帧应当包含 SPS/PPS/IDR，输出非空
    EXPECT_FALSE(nals.empty());
}

TEST(RtpVideoPacketizer, SmallNal) {
    RtpVideoPacketizer pkt;
    NalUnit nal;
    // Annex B start code + 100 字节裸 NAL
    nal.data = {0x00, 0x00, 0x00, 0x01};
    nal.data.push_back(0x65);  // IDR NAL header (type=5)
    for (int i = 0; i < 99; ++i) nal.data.push_back(static_cast<uint8_t>(i & 0xFF));
    nal.is_keyframe = true;

    uint16_t seq = 0;
    auto pkts = pkt.packetize(nal, 96, seq, 1000, 0x12345678);
    EXPECT_EQ(pkts.size(), 1u);
    // 整包 marker=true
    EXPECT_TRUE(pkts[0].marker());
}

TEST(RtpVideoPacketizer, LargeNalFuA) {
    RtpVideoPacketizer pkt;
    NalUnit nal;
    // Annex B start code + 3000 字节裸 NAL
    nal.data = {0x00, 0x00, 0x00, 0x01};
    nal.data.push_back(0x65);  // IDR NAL header
    for (int i = 0; i < 2999; ++i) nal.data.push_back(static_cast<uint8_t>(i & 0xFF));
    nal.is_keyframe = true;

    uint16_t seq = 0;
    auto pkts = pkt.packetize(nal, 96, seq, 1000, 0x12345678);
    ASSERT_GT(pkts.size(), 1u);

    // 验证第一包 FU header 的 S bit
    const uint8_t* first_payload = pkts.front().payload();
    EXPECT_EQ(first_payload[0] & 0x1F, 28u);   // FU indicator type=28
    EXPECT_TRUE(first_payload[1] & 0x80);       // Start bit

    // 验证最后包 FU header 的 E bit
    const uint8_t* last_payload = pkts.back().payload();
    EXPECT_TRUE(last_payload[1] & 0x40);        // End bit
    EXPECT_TRUE(pkts.back().marker());
}
