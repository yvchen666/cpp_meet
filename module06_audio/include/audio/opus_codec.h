#pragma once
#include <cstdint>
#include <vector>
#include <opus/opus.h>

// Opus 编解码器封装（20ms，48kHz，单声道）
class OpusEncoder {
public:
    static constexpr int SAMPLE_RATE  = 48000;
    static constexpr int CHANNELS     = 1;
    static constexpr int FRAME_SIZE   = 960;  // 20ms * 48kHz
    static constexpr int MAX_PKT_SIZE = 4000;

    OpusEncoder();
    ~OpusEncoder();

    bool init(int bitrate_bps = 32000);

    // 输入 FRAME_SIZE 个 int16_t，输出压缩字节
    std::vector<uint8_t> encode(const int16_t* pcm, int frame_size = FRAME_SIZE);

private:
    ::OpusEncoder* enc_{nullptr};
};

class OpusDecoder {
public:
    OpusDecoder();
    ~OpusDecoder();

    bool init();

    // 输入压缩字节，输出最多 FRAME_SIZE 个 int16_t
    std::vector<int16_t> decode(const uint8_t* data, size_t len);

private:
    ::OpusDecoder* dec_{nullptr};
};
