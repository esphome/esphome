#pragma once

#include "esphome/components/camera_video/encoded_stream.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::camera_video {

static constexpr size_t H264_PARAMETER_SET_MAX_SIZE = 128;

struct H264CodecConfig {
  std::array<uint8_t, H264_PARAMETER_SET_MAX_SIZE> sps{};
  size_t sps_length{0};
  std::array<uint8_t, H264_PARAMETER_SET_MAX_SIZE> pps{};
  size_t pps_length{0};
  uint32_t profile_level_id{0};

  bool is_valid() const {
    return this->sps_length >= 4 && this->sps_length <= this->sps.size() && this->pps_length != 0 &&
           this->pps_length <= this->pps.size();
  }
};

/** Shared Annex-B H.264 access unit. Consumers must treat the buffer as read-only. */
class H264Frame : public EncodedVideoFrame {
 public:
  ~H264Frame() override = default;
};

}  // namespace esphome::camera_video
