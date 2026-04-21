#include "video/h264_encoder.h"
#include <cstring>
#include <stdexcept>

H264Encoder::H264Encoder() {
    std::memset(&pic_in_, 0, sizeof(pic_in_));
}

H264Encoder::~H264Encoder() {
    if (enc_) {
        x264_picture_clean(&pic_in_);
        x264_encoder_close(enc_);
        enc_ = nullptr;
    }
}

bool H264Encoder::init(int width, int height, int fps, int bitrate_kbps) {
    width_  = width;
    height_ = height;

    x264_param_t param;
    if (x264_param_default_preset(&param, "ultrafast", "zerolatency") < 0) {
        return false;
    }

    param.i_width       = width;
    param.i_height      = height;
    param.i_fps_num     = static_cast<uint32_t>(fps);
    param.i_fps_den     = 1;
    param.i_keyint_max  = fps * 2;  // 关键帧间隔 2 秒
    param.rc.i_rc_method    = X264_RC_ABR;
    param.rc.i_bitrate      = bitrate_kbps;
    param.i_csp         = X264_CSP_I420;
    param.b_annexb      = 1;  // Annex B 格式
    param.i_log_level   = X264_LOG_NONE;

    if (x264_param_apply_profile(&param, "baseline") < 0) {
        return false;
    }

    if (x264_picture_alloc(&pic_in_, X264_CSP_I420, width, height) < 0) {
        return false;
    }

    enc_ = x264_encoder_open(&param);
    return enc_ != nullptr;
}

std::vector<NalUnit> H264Encoder::encode(const YuvFrame& frame) {
    if (!enc_) return {};

    // 复制 YUV 数据到 pic_in_
    int y_size  = width_ * height_;
    int uv_size = y_size / 4;
    if (static_cast<int>(frame.data.size()) < y_size + 2 * uv_size) return {};

    std::memcpy(pic_in_.img.plane[0], frame.data.data(),              y_size);
    std::memcpy(pic_in_.img.plane[1], frame.data.data() + y_size,     uv_size);
    std::memcpy(pic_in_.img.plane[2], frame.data.data() + y_size + uv_size, uv_size);

    pic_in_.i_pts = static_cast<int64_t>(frame.timestamp_ms);

    x264_picture_t pic_out;
    x264_nal_t*    nals    = nullptr;
    int            nal_cnt = 0;

    int frame_size = x264_encoder_encode(enc_, &nals, &nal_cnt, &pic_in_, &pic_out);
    if (frame_size < 0) return {};

    std::vector<NalUnit> result;
    for (int i = 0; i < nal_cnt; ++i) {
        NalUnit nu;
        nu.data.assign(nals[i].p_payload,
                       nals[i].p_payload + nals[i].i_payload);
        nu.is_keyframe = (pic_out.b_keyframe != 0);
        result.push_back(std::move(nu));
    }
    return result;
}
