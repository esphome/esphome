#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <atomic>
#include <memory>
#include "esphome/components/camera_video/encoded_stream.h"
#include "esphome/components/camera_video/legacy_jpeg_stream.h"
#include "esphome/core/helpers.h"
#ifdef USE_SOCKET_IPV4_ALLOW
#include "esphome/components/socket/ipv4_allow.h"
#endif
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "rtsp_session.h"

namespace esphome::rtsp_server {

class RTSPServer : public Component, public camera_video::EncodedVideoStreamListener {
 public:
  explicit RTSPServer(uint16_t port) : port_(port) {}
  RTSPServer(uint16_t port, camera_video::EncodedVideoStream *stream) : stream_(stream), port_(port) {}
  void set_rtp_payload_size(uint16_t payload_size) { this->payload_size_ = payload_size; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override;

  void on_encoded_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) override;
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const socket::Ipv4AllowEntry *entries, size_t count) { this->allow_.set(entries, count); }
#endif

 protected:
  void accept_new_connections_(uint32_t now);
  void socket_failed_(const char *what);
  void set_subscription_(bool enable);
  void clear_pending_frame_();
  camera_video::FrameRef<camera_video::EncodedVideoFrame> take_pending_frame_();
  socket::ListenSocket *socket_{nullptr};
  std::array<RTSPSession, MAX_RTSP_CONNECTIONS> sessions_{};
#ifdef USE_SOCKET_IPV4_ALLOW
  socket::Ipv4Allow allow_;
#endif
  // The JPEG adapter cannot unregister from camera::Camera and must outlive registration.
  std::unique_ptr<camera_video::LegacyJpegStreamAdapter> jpeg_adapter_;
  camera_video::EncodedVideoStream *stream_{nullptr};
  Mutex frame_mutex_;
  camera_video::FrameRef<camera_video::EncodedVideoFrame> pending_frame_;
  std::atomic<bool> discontinuity_{false};
  uint16_t payload_size_{1400};
  bool subscribed_{false};
  bool priming_{false};
  uint16_t port_;
  bool streaming_{false};
};

}  // namespace esphome::rtsp_server
#endif
