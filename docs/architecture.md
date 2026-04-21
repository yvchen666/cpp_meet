# 架构总览

## 系统组件

```
┌─────────────────────────────────────────────────────────────────┐
│                        Client A (Qt6)                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ V4L2     │  │ ALSA     │  │  ICE/    │  │  Qt UI +     │   │
│  │ Capture  │  │ Capture  │  │  DTLS    │  │  OpenGL YUV  │   │
│  └────┬─────┘  └────┬─────┘  │  SRTP    │  └──────────────┘   │
│       │ YUV         │ PCM    └────┬─────┘                       │
│  ┌────▼─────┐  ┌────▼─────┐      │ UDP                         │
│  │ H264 Enc │  │ Opus Enc │      │                              │
│  └────┬─────┘  └────┬─────┘      │                              │
│       └──────────────┘           │                              │
│              RTP + FEC ──────────┘                              │
└─────────────────────────────────────────────────────────────────┘
                         │ UDP (SRTP)
                ┌────────▼────────┐
                │   SFU Server   │
                │  module09_sfu  │
                │  ICE+DTLS+SRTP │
                │  路由 + SSRC重写│
                └────────┬────────┘
                         │ UDP (SRTP)
┌─────────────────────────────────────────────────────────────────┐
│                        Client B (Qt6)                           │
└─────────────────────────────────────────────────────────────────┘
```

## 信令流程（WebSocket）

```
Client A          Signaling Server        Client B
   │                    │                    │
   │── join(room_id) ──▶│                    │
   │◀── room_info ──────│                    │
   │                    │◀── join(room_id) ──│
   │◀── peer_joined ────│                    │
   │── offer(SDP) ─────▶│── offer(SDP) ─────▶│
   │                    │◀── answer(SDP) ────│
   │◀── answer(SDP) ────│                    │
   │── ice_candidate ──▶│── ice_candidate ──▶│
```

## 模块依赖图

```
module01_network_core
    ├── module02_rtp_rtcp
    │       ├── module06_audio
    │       ├── module07_video
    │       └── module08_weak_network
    ├── module03_stun_ice
    │       └── module04_dtls_srtp
    │               └── module09_sfu
    └── module05_signaling
            └── module09_sfu

module09_sfu + module06_audio + module07_video + module08_weak_network
    └── module10_qt_client
            └── module11_whiteboard
                    └── module12_integration
```
