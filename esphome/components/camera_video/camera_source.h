#pragma once

#include "esphome/components/camera/buffer.h"
#include "esphome/components/camera_video/frame_ref.h"

#include <cstddef>
#include <cstdint>

namespace esphome::camera_video {

enum class VideoPixelFormat : uint8_t {
  VIDEO_PIXEL_FORMAT_O_UYY_E_VYY,
  VIDEO_PIXEL_FORMAT_VUY,
  VIDEO_PIXEL_FORMAT_UYVY,
  VIDEO_PIXEL_FORMAT_BGR888,
  VIDEO_PIXEL_FORMAT_RGB565_LE,
};

struct CameraVideoFrameSpec {
  uint16_t width{0};
  uint16_t height{0};
  uint16_t fps{0};
  VideoPixelFormat format{VideoPixelFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY};
  size_t stride{0};
  size_t buffer_size{0};

  size_t packed_row_bytes() const {
    switch (this->format) {
      case VideoPixelFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY:
        return (static_cast<size_t>(this->width) * 3U) / 2U;
      case VideoPixelFormat::VIDEO_PIXEL_FORMAT_VUY:
      case VideoPixelFormat::VIDEO_PIXEL_FORMAT_BGR888:
        return static_cast<size_t>(this->width) * 3U;
      case VideoPixelFormat::VIDEO_PIXEL_FORMAT_UYVY:
      case VideoPixelFormat::VIDEO_PIXEL_FORMAT_RGB565_LE:
        return static_cast<size_t>(this->width) * 2U;
    }
    return 0;
  }

  bool is_complete() const {
    const size_t minimum_stride = this->packed_row_bytes();
    if (this->width == 0 || this->height == 0 || minimum_stride == 0 || this->stride < minimum_stride ||
        this->buffer_size < this->stride * this->height) {
      return false;
    }
    if (this->format == VideoPixelFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY) {
      return (this->width & 1U) == 0 && (this->height & 1U) == 0;
    }
    if (this->format == VideoPixelFormat::VIDEO_PIXEL_FORMAT_UYVY) {
      return (this->width & 1U) == 0;
    }
    return true;
  }
};

/** Shared raw video frame. Consumers must treat the buffer as read-only. */
class CameraVideoFrame : public camera::Buffer, public RefCountedFrame {
 public:
  /** Monotonic capture timestamp supplied by the raw source. */
  virtual uint64_t get_timestamp_us() const = 0;
  ~CameraVideoFrame() override = default;
};

class CameraVideoSourceListener {
 public:
  /** Source-task callback; implementations must not block. */
  virtual void on_video_frame(const FrameRef<CameraVideoFrame> &frame) = 0;
  virtual ~CameraVideoSourceListener() = default;
};

/** Raw-video producer with listener-scoped stream ownership. */
class CameraVideoSource {
 public:
  /** Returns false until the source has a complete, stable frame contract. */
  virtual bool get_video_frame_spec(CameraVideoFrameSpec *spec) const = 0;
  /** Registers one consumer and starts frame delivery for that identity. */
  virtual bool start_stream(CameraVideoSourceListener *listener) = 0;
  /** Stops frame delivery only for the supplied consumer identity. */
  virtual void stop_stream(CameraVideoSourceListener *listener) = 0;
  virtual ~CameraVideoSource() = default;
};

}  // namespace esphome::camera_video
