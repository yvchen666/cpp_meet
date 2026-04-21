# cpp_meet — 从零实现类腾讯会议桌面软件

纯 C++17 全栈视频会议系统，客户端 + SFU 服务器，学习向实现（不依赖 libwebrtc）。

## 快速开始

```bash
# 安装系统依赖
sudo apt install libopus-dev libx264-dev \
    libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev libavutil-dev \
    libssl-dev qt6-base-dev qt6-base-dev-tools \
    libasound2-dev libv4l-dev pkg-config

# 构建
cd /home/aoi/AWorkSpace/cpp_meet
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# 测试
cd build && ctest --output-on-failure
```

## 模块列表

| 模块 | 功能 |
|------|------|
| module01_network_core  | UDP/TCP socket + epoll事件循环 + 无锁SPSC队列 |
| module02_rtp_rtcp      | RTP/RTCP 协议编解码（RFC 3550） |
| module03_stun_ice      | STUN（RFC 5389）+ ICE（RFC 8445） |
| module04_dtls_srtp     | DTLS握手 + SRTP加密（RFC 5764） |
| module05_signaling     | WebSocket服务器 + SDP解析 + 房间管理 |
| module06_audio         | 麦克风采集 + Opus编解码 + ALSA播放 |
| module07_video         | V4L2摄像头 + H.264编码 + RTP打包 |
| module08_weak_network  | JitterBuffer + XOR-FEC + NACK + GCC |
| module09_sfu           | 选择性转发单元 |
| module10_qt_client     | Qt6桌面客户端 + YUV OpenGL渲染 |
| module11_whiteboard    | 协同白板（CRDT LWW-Element-Set） |
| module12_integration   | 端到端集成测试 |

## 文档

- [架构总览](docs/architecture.md)
- [协议流程](docs/protocol_flow.md)
- [线程模型](docs/threading_model.md)
