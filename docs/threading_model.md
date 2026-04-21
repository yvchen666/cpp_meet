# 线程模型

## 客户端线程架构（7-8个线程）

```
┌──────────────────────────────────────────────────────────────────┐
│ 音频采集线程  AudioCapturer → RingBuffer<PCMFrame,1024>          │
│ 视频采集线程  VideoCapturer → RingBuffer<YUVFrame,8>             │
│ 音频编码线程  OpusEncoder → RTP打包 → FEC → NetworkThread        │
│ 视频编码线程  H264Encoder → RTP打包 → FEC → NetworkThread        │
│ 网络IO线程   epoll: ICE心跳 + DTLS驱动 + SRTP收发               │
│ 解码线程     JitterBuffer → Opus/H264解码 → Qt渲染队列           │
│ Qt主线程     UI事件 + OpenGL渲染 (QueuedConnection)              │
│ 音频输出线程  ALSA播放 snd_pcm_writei 实时循环                   │
└──────────────────────────────────────────────────────────────────┘
```

## 跨线程通信机制

| 路径 | 机制 | 理由 |
|------|------|------|
| 采集→编码 | SPSC RingBuffer (无锁) | 热路径，零copy，无mutex |
| 编码→网络 | SPSC RingBuffer (无锁) | 同上 |
| 网络→解码 | SPSC RingBuffer (无锁) | 同上 |
| 解码→Qt渲染 | Qt::QueuedConnection | Qt事件循环线程安全投递 |
| 控制命令 | std::mutex + std::condition_variable | 低频，简单 |

## SPSC RingBuffer 设计

```cpp
// head: 只由生产者写，消费者读
// tail: 只由消费者写，生产者读
// 满足: (head - tail + N) % N < N  →  不满
// 注意: head/tail 不取模，只比较差值，避免伪共享
template<typename T, size_t N>
class RingBuffer {
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    T buf_[N];
};
```

## 网络IO线程事件循环

```
epoll_wait(timeout=1ms)
  → EPOLLIN on udp_fd
      → recv udp packet
      → demux: STUN / DTLS / SRTP
          STUN  → ice_agent.on_stun_packet()
          DTLS  → ssl_bio.feed() → SSL_do_handshake()
          SRTP  → srtp_unprotect() → push to decode ring
  → timeout
      → ice_agent.send_keepalive() (每15s一次binding request)
      → dtls.drive_timeout() (DTLSv1_handle_timeout)
```
