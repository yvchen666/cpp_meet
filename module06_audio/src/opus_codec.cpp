#include "audio/opus_codec.h"
#include <stdexcept>
#include <cstring>

// ──────────────────────────────────────────────
// OpusEncoder
// ──────────────────────────────────────────────

OpusEncoder::OpusEncoder() = default;

OpusEncoder::~OpusEncoder() {
    if (enc_) {
        opus_encoder_destroy(enc_);
        enc_ = nullptr;
    }
}

bool OpusEncoder::init(int bitrate_bps) {
    int err = OPUS_OK;
    enc_ = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !enc_) {
        return false;
    }
    if (opus_encoder_ctl(enc_, OPUS_SET_BITRATE(bitrate_bps)) != OPUS_OK) {
        opus_encoder_destroy(enc_);
        enc_ = nullptr;
        return false;
    }
    return true;
}

std::vector<uint8_t> OpusEncoder::encode(const int16_t* pcm, int frame_size) {
    if (!enc_) return {};
    std::vector<uint8_t> out(MAX_PKT_SIZE);
    opus_int32 bytes = opus_encode(enc_, pcm, frame_size, out.data(),
                                   static_cast<opus_int32>(out.size()));
    if (bytes < 0) return {};
    out.resize(static_cast<size_t>(bytes));
    return out;
}

// ──────────────────────────────────────────────
// OpusDecoder
// ──────────────────────────────────────────────

OpusDecoder::OpusDecoder() = default;

OpusDecoder::~OpusDecoder() {
    if (dec_) {
        opus_decoder_destroy(dec_);
        dec_ = nullptr;
    }
}

bool OpusDecoder::init() {
    int err = OPUS_OK;
    dec_ = opus_decoder_create(OpusEncoder::SAMPLE_RATE, OpusEncoder::CHANNELS, &err);
    return (err == OPUS_OK && dec_ != nullptr);
}

std::vector<int16_t> OpusDecoder::decode(const uint8_t* data, size_t len) {
    if (!dec_) return {};
    std::vector<int16_t> out(OpusEncoder::FRAME_SIZE);
    int samples = opus_decode(dec_, data, static_cast<opus_int32>(len),
                              out.data(), OpusEncoder::FRAME_SIZE, 0);
    if (samples < 0) return {};
    out.resize(static_cast<size_t>(samples));
    return out;
}
