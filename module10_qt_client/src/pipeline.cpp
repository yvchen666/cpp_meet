#include "qtclient/pipeline.h"
#include <thread>
#include <chrono>

MediaPipeline::MediaPipeline(QObject* parent)
    : QObject(parent)
{}

MediaPipeline::~MediaPipeline() {
    stop();
}

void MediaPipeline::start() {
    if (running_.load()) return;
    running_ = true;
    worker_ = std::thread(&MediaPipeline::run, this);
}

void MediaPipeline::stop() {
    running_ = false;
    if (worker_.joinable())
        worker_.join();
}

void MediaPipeline::run() {
    constexpr int W = 320;
    constexpr int H = 240;
    int frame_idx = 0;

    while (running_.load()) {
        // 生成 YUV420P 测试帧（简单灰度渐变）
        QByteArray y_plane(W * H, 0);
        QByteArray u_plane(W / 2 * H / 2, 128);
        QByteArray v_plane(W / 2 * H / 2, 128);

        uint8_t* y_data = reinterpret_cast<uint8_t*>(y_plane.data());
        for (int i = 0; i < W * H; ++i) {
            y_data[i] = static_cast<uint8_t>((frame_idx * 4 + i) & 0xFF);
        }

        emit frameReady(W, H, std::move(y_plane), std::move(u_plane), std::move(v_plane));

        ++frame_idx;
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}
