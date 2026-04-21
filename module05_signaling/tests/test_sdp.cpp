#include <gtest/gtest.h>
#include "sig/sdp.h"
#include <optional>

// -----------------------------------------------------------------------
// 辅助：构建一个典型 SDP 字符串
// -----------------------------------------------------------------------

static std::string make_sample_sdp() {
    return
        "v=0\r\n"
        "o=- 1234567890 2 IN IP4 127.0.0.1\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "m=audio 9 UDP/TLS/RTP/SAVPF 111 103\r\n"
        "a=ice-ufrag:ufrag_audio\r\n"
        "a=ice-pwd:pwd_audio_long_enough_here\r\n"
        "a=fingerprint:sha-256 AA:BB:CC:DD:EE:FF:00:11\r\n"
        "a=candidate:host 1 udp 2130706431 192.168.1.1 5000 typ host\r\n"
        "a=rtpmap:111 opus/48000/2\r\n"
        "a=rtpmap:103 ISAC/16000\r\n"
        "m=video 9 UDP/TLS/RTP/SAVPF 96 97\r\n"
        "a=ice-ufrag:ufrag_video\r\n"
        "a=ice-pwd:pwd_video_long_enough_here\r\n"
        "a=fingerprint:sha-256 11:22:33:44:55:66:77:88\r\n"
        "a=rtpmap:96 VP8/90000\r\n"
        "a=rtpmap:97 H264/90000\r\n";
}

// -----------------------------------------------------------------------
// TEST: Parse
// -----------------------------------------------------------------------

TEST(Sdp, Parse) {
    auto result = Sdp::parse(make_sample_sdp());
    ASSERT_TRUE(result.has_value());

    const Sdp& sdp = *result;
    EXPECT_EQ(sdp.version, "0");
    EXPECT_EQ(sdp.session_name, "-");
    ASSERT_EQ(sdp.media.size(), 2u);

    // audio
    const SdpMedia& audio = sdp.media[0];
    EXPECT_EQ(audio.type, "audio");
    EXPECT_EQ(audio.port, 9);
    EXPECT_EQ(audio.proto, "UDP/TLS/RTP/SAVPF");
    ASSERT_EQ(audio.fmts.size(), 2u);
    EXPECT_EQ(audio.fmts[0], 111);
    EXPECT_EQ(audio.fmts[1], 103);
    EXPECT_EQ(audio.ice_ufrag, "ufrag_audio");
    EXPECT_EQ(audio.ice_pwd, "pwd_audio_long_enough_here");
    EXPECT_EQ(audio.fingerprint, "sha-256 AA:BB:CC:DD:EE:FF:00:11");
    EXPECT_FALSE(audio.candidate.empty());
    ASSERT_EQ(audio.rtpmap.size(), 2u);
    EXPECT_EQ(audio.rtpmap[0].first, 111);
    EXPECT_EQ(audio.rtpmap[0].second, "opus/48000/2");
    EXPECT_EQ(audio.rtpmap[1].first, 103);
    EXPECT_EQ(audio.rtpmap[1].second, "ISAC/16000");

    // video
    const SdpMedia& video = sdp.media[1];
    EXPECT_EQ(video.type, "video");
    ASSERT_EQ(video.rtpmap.size(), 2u);
    EXPECT_EQ(video.rtpmap[0].second, "VP8/90000");
}

// -----------------------------------------------------------------------
// TEST: Serialize
// -----------------------------------------------------------------------

TEST(Sdp, Serialize) {
    auto parsed = Sdp::parse(make_sample_sdp());
    ASSERT_TRUE(parsed.has_value());

    std::string serialized = parsed->serialize();
    EXPECT_NE(serialized.find("v=0"), std::string::npos);
    EXPECT_NE(serialized.find("m=audio"), std::string::npos);
    EXPECT_NE(serialized.find("m=video"), std::string::npos);
    EXPECT_NE(serialized.find("a=rtpmap:111 opus/48000/2"), std::string::npos);
    EXPECT_NE(serialized.find("a=ice-ufrag:ufrag_audio"), std::string::npos);
}

// -----------------------------------------------------------------------
// TEST: ParseAndSerialize（往返一致性）
// -----------------------------------------------------------------------

TEST(Sdp, ParseAndSerialize) {
    // 1. 构造 SDP 字符串
    std::string original = make_sample_sdp();

    // 2. parse
    auto sdp_opt = Sdp::parse(original);
    ASSERT_TRUE(sdp_opt.has_value());
    Sdp& sdp = *sdp_opt;

    // 3. 修改：追加一个 rtpmap
    sdp.media[0].rtpmap.emplace_back(9, "G722/8000");

    // 4. serialize
    std::string serialized = sdp.serialize();
    EXPECT_NE(serialized.find("a=rtpmap:9 G722/8000"), std::string::npos);

    // 5. 再次 parse，验证一致
    auto sdp2_opt = Sdp::parse(serialized);
    ASSERT_TRUE(sdp2_opt.has_value());
    const Sdp& sdp2 = *sdp2_opt;

    ASSERT_EQ(sdp2.media.size(), sdp.media.size());
    EXPECT_EQ(sdp2.media[0].type, sdp.media[0].type);
    EXPECT_EQ(sdp2.media[0].rtpmap.size(), sdp.media[0].rtpmap.size());

    // 最后一个 rtpmap 应是我们追加的 G722
    const auto& last_rtpmap = sdp2.media[0].rtpmap.back();
    EXPECT_EQ(last_rtpmap.first, 9);
    EXPECT_EQ(last_rtpmap.second, "G722/8000");
}

// -----------------------------------------------------------------------
// TEST: EmptySdp
// -----------------------------------------------------------------------

TEST(Sdp, EmptySdp) {
    // 空字符串解析应返回空 Sdp（不 crash）
    auto result = Sdp::parse("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->media.empty());
}
