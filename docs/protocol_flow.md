# 协议流程

## 完整呼叫建立流程

```mermaid
sequenceDiagram
    participant A
    participant Signal
    participant SFU
    participant B

    A->>Signal: WS connect
    A->>Signal: join(room)
    Signal-->>A: room_info
    B->>Signal: WS connect
    B->>Signal: join(room)
    Signal-->>A: peer_joined

    Note over A: 创建 ICE Agent, 收集候选

    A->>Signal: offer(SDP)
    Signal->>SFU: offer(SDP)
    Signal->>B: offer(SDP)
    B->>Signal: answer(SDP)
    Signal-->>A: answer(SDP)

    Note over A,SFU: STUN Binding Request/Response
    A->>SFU: STUN
    SFU-->>A: STUN

    Note over A,SFU: DTLS握手
    A->>SFU: DTLS ClientHello
    SFU-->>A: DTLS ServerHello+Cert
    A->>SFU: DTLS Certificate
    SFU-->>A: DTLS Finished

    Note over A,SFU: SRTP密钥导出, 开始媒体
    A->>SFU: SRTP(RTP音频/视频)
    SFU->>B: SRTP(RTP音频/视频)
    B->>SFU: SRTP(RTP音频/视频)
    SFU->>A: SRTP(RTP音频/视频)
```

## RTP/RTCP 包格式

```
RTP Header (最小12字节):
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
│V=2│P│X│  CC   │M│    PT     │         Sequence Number           │
├───────────────────────────────────────────────────────────────────┤
│                           Timestamp                               │
├───────────────────────────────────────────────────────────────────┤
│                             SSRC                                  │
└───────────────────────────────────────────────────────────────────┘
```

## DTLS字节复用(demux)

```
首字节范围:
  [0,  3]  → STUN
  [20, 63] → DTLS
  [128,191]→ SRTP/SRTCP
```
