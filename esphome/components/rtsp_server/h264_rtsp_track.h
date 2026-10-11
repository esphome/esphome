#pragma once
#ifdef USE_RTSP_SERVER

#include "esphome/components/camera_video/h264_stream.h"
#include "esphome/core/helpers.h"
#include "rtsp_track.h"

#include <atomic>

namespace esphome::rtsp_server {

class H264RtspTrack final : public RtspTrack, public camera_video::H264StreamListener {
 public:
  explicit H264RtspTrack(camera_video::H264Stream *stream) : stream_(stream) {}
  ~H264RtspTrack() override;

  bool start(RtspTrackListener *listener) override;
  void stop(RtspTrackListener *listener) override;
  bool is_ready() const override;
  bool is_sdp_ready() const override;
  const char *get_codec_name() const override { return "H.264"; }
  uint32_t get_frame_period_ms() const override;
  bool write_sdp(char *buffer, size_t buffer_size, const char *control_path) const override;
  RtspTrackSendResult send_next_access_unit(RtpPacketSink &sink) override;
  void purge() override;
  void request_keyframe() override;

  void on_h264_frame(const camera_video::FrameRef<camera_video::H264Frame> &frame) override;

 protected:
  camera_video::FrameRef<camera_video::H264Frame> take_pending_frame_();
  bool send_nal_(RtpPacketSink &sink, const uint8_t *nal, size_t length, bool marker, bool startup_config) const;

  camera_video::H264Stream *stream_{nullptr};
  std::atomic<RtspTrackListener *> listener_{nullptr};
  std::atomic<bool> subscribed_{false};
  Mutex frame_mutex_;
  camera_video::FrameRef<camera_video::H264Frame> pending_frame_;
  std::atomic<uint32_t> dropped_frames_{0};
};

}  // namespace esphome::rtsp_server
#endif
