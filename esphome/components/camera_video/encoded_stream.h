#pragma once

#include "esphome/components/camera/buffer.h"
#include "esphome/components/camera_video/frame_ref.h"

#include <cstddef>
#include <cstdint>

namespace esphome::camera_video {

/** Codec identifiers are transport-independent; RTP payload types are not codecs. */
enum class VideoCodec : uint8_t { JPEG, H264 };

/** Information that remains stable for the lifetime of a stream configuration. */
struct EncodedVideoFormat {
  VideoCodec codec{VideoCodec::JPEG};
  uint16_t width{0};
  uint16_t height{0};
  uint32_t fps_num{0};
  uint32_t fps_den{1};

  bool is_valid() const { return this->width != 0 && this->height != 0 && this->fps_num != 0 && this->fps_den != 0; }
};

/** Producer-owned encoded access unit; all listeners treat its bytes as immutable. */
class EncodedVideoFrame : public camera::Buffer, public RefCountedFrame {
 public:
  virtual uint64_t get_timestamp_us() const = 0;
  virtual bool is_keyframe() const = 0;
  ~EncodedVideoFrame() override = default;
};

class EncodedVideoStreamListener {
 public:
  /** Called on a producer task: do not block or retain raw pointers without a FrameRef. */
  virtual void on_encoded_frame(const FrameRef<EncodedVideoFrame> &frame) = 0;
  virtual ~EncodedVideoStreamListener() = default;
};

/** Does not own the listener; callers must unsubscribe before destruction. */
class EncodedVideoStream {
 public:
  virtual bool get_format(EncodedVideoFormat *format) const = 0;
  /** Returns codec-specific configuration; JPEG streams may have no extradata. */
  virtual bool get_codec_config(uint8_t *buffer, size_t capacity, size_t *length) const = 0;
  virtual bool start_stream(EncodedVideoStreamListener *listener) = 0;
  virtual void stop_stream(EncodedVideoStreamListener *listener) = 0;
  virtual bool is_ready() const = 0;
  /** Unsupported by some software encoders; callers must tolerate false. */
  virtual bool request_keyframe() { return false; }
  virtual ~EncodedVideoStream() = default;
};

}  // namespace esphome::camera_video
