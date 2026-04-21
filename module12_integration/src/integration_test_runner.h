// 端到端集成测试辅助：合成音视频源
#pragma once
#include <cstdint>
#include <vector>

struct YuvFrame {
    int width{0};
    int height{0};
    std::vector<uint8_t> data;  // YUV420P planar
    int64_t timestamp_ms{0};
};

// 合成视频帧生成器（简单渐变 YUV）
class SyntheticVideoSource {
public:
    YuvFrame next_frame(int frame_idx) {
        YuvFrame f;
        f.width = 320;
        f.height = 240;
        f.data.resize(320 * 240 * 3 / 2, 0);
        // Y 平面：简单渐变
        for (int i = 0; i < 320 * 240; ++i)
            f.data[i] = static_cast<uint8_t>((frame_idx * 4 + i) & 0xFF);
        f.timestamp_ms = frame_idx * 33;
        return f;
    }
};
