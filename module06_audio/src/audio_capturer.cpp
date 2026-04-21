#include "audio/audio_capturer.h"
#include <fstream>
#include <chrono>
#include <thread>

AudioCapturerFile::AudioCapturerFile(const std::string& path)
    : path_(path) {}

AudioCapturerFile::~AudioCapturerFile() {
    stop();
}

void AudioCapturerFile::start() {
    running_ = true;
    thread_ = std::thread([this]() {
        std::ifstream file(path_, std::ios::binary);
        // 允许文件不存在（静默退出），用于无文件的 CI 场景
        uint32_t ts_ms = 0;
        constexpr int FRAME_SAMPLES = 960;
        constexpr int FRAME_MS = 20;

        while (running_) {
            PcmFrame frame;
            frame.samples.resize(FRAME_SAMPLES);
            frame.timestamp_ms = ts_ms;

            if (file.is_open()) {
                file.read(reinterpret_cast<char*>(frame.samples.data()),
                          FRAME_SAMPLES * sizeof(int16_t));
                auto got = file.gcount();
                if (got <= 0) {
                    // 文件读完，循环到开头
                    file.clear();
                    file.seekg(0, std::ios::beg);
                    file.read(reinterpret_cast<char*>(frame.samples.data()),
                              FRAME_SAMPLES * sizeof(int16_t));
                    got = file.gcount();
                }
                auto samples_got = static_cast<size_t>(got) / sizeof(int16_t);
                frame.samples.resize(samples_got);
                // 补零至 FRAME_SAMPLES
                frame.samples.resize(FRAME_SAMPLES, 0);
            } else {
                // 无文件：产生静音帧
                std::fill(frame.samples.begin(), frame.samples.end(), 0);
            }

            if (cb_) cb_(frame);

            ts_ms += FRAME_MS;
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_MS));
        }
    });
}

void AudioCapturerFile::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}
