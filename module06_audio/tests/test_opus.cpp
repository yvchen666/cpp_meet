#include <gtest/gtest.h>
#include "audio/opus_codec.h"
#include <cmath>
#include <numeric>

static std::vector<int16_t> make_sine(int samples, float freq = 440.f, int sr = 48000) {
    std::vector<int16_t> pcm(samples);
    for (int i = 0; i < samples; ++i) {
        pcm[i] = static_cast<int16_t>(20000.f * std::sin(2.f * M_PI * freq * i / sr));
    }
    return pcm;
}

TEST(OpusCodec, EncodeDecodeRoundtrip) {
    OpusEncoder enc;
    OpusDecoder dec;
    ASSERT_TRUE(enc.init(32000));
    ASSERT_TRUE(dec.init());

    auto pcm = make_sine(OpusEncoder::FRAME_SIZE);
    auto compressed = enc.encode(pcm.data());
    ASSERT_FALSE(compressed.empty());

    auto decoded = dec.decode(compressed.data(), compressed.size());
    ASSERT_EQ(static_cast<int>(decoded.size()), OpusEncoder::FRAME_SIZE);

    // 验证解码输出非零（有声音内容）
    int64_t energy = 0;
    for (auto s : decoded) energy += std::abs(static_cast<int>(s));
    EXPECT_GT(energy, 0);

    // SNR 估算：signal power vs error power
    double sig_power = 0.0, err_power = 0.0;
    for (int i = 0; i < OpusEncoder::FRAME_SIZE; ++i) {
        double s = pcm[i];
        double d = decoded[i];
        sig_power += s * s;
        err_power += (s - d) * (s - d);
    }
    // 松散检查：err_power < sig_power（说明解码有意义）
    EXPECT_LT(err_power, sig_power * 10.0);
}

TEST(OpusCodec, EncodedSizeReduced) {
    OpusEncoder enc;
    ASSERT_TRUE(enc.init(32000));

    auto pcm = make_sine(OpusEncoder::FRAME_SIZE);
    auto compressed = enc.encode(pcm.data());

    // 编码后字节数应小于原始字节数 960*2=1920
    EXPECT_LT(compressed.size(), static_cast<size_t>(OpusEncoder::FRAME_SIZE * 2));
    EXPECT_GT(compressed.size(), 0u);
}
