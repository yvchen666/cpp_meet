#pragma once
#include <thread>
#include <atomic>
#include <QObject>

// 媒体处理流水线（非 Qt UI 部分）
// 管理编解码和网络线程
class MediaPipeline : public QObject {
    Q_OBJECT
public:
    explicit MediaPipeline(QObject* parent = nullptr);
    ~MediaPipeline();

    void start();
    void stop();

signals:
    void frameReady(int width, int height, QByteArray y, QByteArray u, QByteArray v);

private:
    std::thread worker_;
    std::atomic<bool> running_{false};
    void run();
};
