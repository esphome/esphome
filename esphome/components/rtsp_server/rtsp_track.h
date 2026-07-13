#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::rtsp_server {

/** Scatter/gather RTP payload view. The sink must consume the pointers synchronously and must not retain them. */
struct RtpPacketView {
  uint8_t payload_type{0};
  const uint8_t *prefix{nullptr};
  size_t prefix_length{0};
  const uint8_t *payload{nullptr};
  size_t payload_length{0};
  bool marker{false};
  bool startup_config{false};
};

class RtpPacketSink {
 public:
  virtual size_t get_maximum_payload_size() const = 0;
  virtual bool begin_access_unit(uint32_t timestamp, bool keyframe, size_t data_length) = 0;
  virtual bool send_packet(const RtpPacketView &packet) = 0;
  virtual bool end_access_unit() = 0;
  virtual ~RtpPacketSink() = default;
};

class RtspTrackListener {
 public:
  virtual void on_rtsp_track_frame_available() = 0;
  virtual ~RtspTrackListener() = default;
};

enum class RtspTrackSendResult : uint8_t {
  RTSP_TRACK_SEND_RESULT_NO_FRAME,
  RTSP_TRACK_SEND_RESULT_SKIPPED,
  RTSP_TRACK_SEND_RESULT_SENT,
  RTSP_TRACK_SEND_RESULT_FAILED,
};

class RtspTrack {
 public:
  virtual bool start(RtspTrackListener *listener) = 0;
  virtual void stop(RtspTrackListener *listener) = 0;
  virtual bool is_ready() const = 0;
  virtual bool is_sdp_ready() const = 0;
  virtual const char *get_codec_name() const = 0;
  virtual uint32_t get_frame_period_ms() const = 0;
  virtual bool write_sdp(char *buffer, size_t buffer_size, const char *control_path) const = 0;
  virtual RtspTrackSendResult send_next_access_unit(RtpPacketSink &sink) = 0;
  virtual void purge() = 0;
  virtual void request_keyframe() {}
  virtual ~RtspTrack() = default;
};

}  // namespace esphome::rtsp_server
