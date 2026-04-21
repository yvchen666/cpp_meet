#pragma once
#include <cstdint>
#include <vector>
#include <functional>
extern "C" {
#include <x264.h>
}

struct YuvFrame {
    int width, height;
    std::vector<uint8_t> data;  // YUV420P 平面格式
    uint32_t timestamp_ms;
};

struct NalUnit {
    std::vector<uint8_t> data;  // 含 Annex B start code
    bool is_keyframe;
};

// H.264 编码器（x264 封装）
class H264Encoder {
public:
    H264Encoder();
    ~H264Encoder();

    bool init(int width, int height, int fps = 30, int bitrate_kbps = 1000);

    // 编码一帧，返回 NAL units（Annex B 格式）
    std::vector<NalUnit> encode(const YuvFrame& frame);

private:
    x264_t*        enc_{nullptr};
    x264_picture_t pic_in_;
    int width_{0}, height_{0};
};
