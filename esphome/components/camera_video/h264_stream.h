#pragma once

#include "esphome/components/camera/buffer.h"
#include "esphome/components/camera_video/frame_ref.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::camera_video {

static constexpr size_t H264_PARAMETER_SET_MAX_SIZE = 128;

struct H264StreamInfo {
  uint16_t width{0};
  uint16_t height{0};
  uint16_t fps{0};
  uint32_t bitrate{0};
};

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
class H264Frame : public camera::Buffer, public RefCountedFrame {
 public:
  virtual uint64_t get_timestamp_us() const = 0;
  virtual bool is_keyframe() const = 0;
  ~H264Frame() override = default;
};

class H264StreamListener {
 public:
  /** Encoder-task callback; must not block. */
  virtual void on_h264_frame(const FrameRef<H264Frame> &frame) = 0;
  virtual ~H264StreamListener() = default;
};

class H264Stream {
 public:
  virtual bool start_stream(H264StreamListener *listener) = 0;
  virtual void stop_stream(H264StreamListener *listener) = 0;
  virtual bool is_ready() const = 0;
  virtual H264StreamInfo get_stream_info() const = 0;
  virtual bool get_h264_codec_config(H264CodecConfig *config) const = 0;
  /** Request that the next encoded frame be an IDR without changing the configured GOP. */
  virtual void request_keyframe() = 0;
  virtual ~H264Stream() = default;
};

}  // namespace esphome::camera_video
