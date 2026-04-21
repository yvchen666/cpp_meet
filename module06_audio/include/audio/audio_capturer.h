#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

// PCM 帧（20ms，48kHz，单声道）
struct PcmFrame {
    std::vector<int16_t> samples;  // 960 个采样
    uint32_t timestamp_ms;
};

// 文件音频源（读 raw PCM，用于CI测试，无硬件依赖）
class AudioCapturerFile {
public:
    using FrameCb = std::function<void(const PcmFrame&)>;

    explicit AudioCapturerFile(const std::string& path);
    ~AudioCapturerFile();

    void on_frame(FrameCb cb) { cb_ = cb; }
    void start();
    void stop();

private:
    std::string path_;
    FrameCb cb_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
