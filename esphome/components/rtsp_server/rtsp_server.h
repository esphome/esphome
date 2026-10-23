#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <atomic>
#include <memory>
#include "esphome/components/camera/camera.h"
#ifdef USE_RTSP_H264
#include "esphome/components/camera_video/h264_stream.h"
#include "esphome/core/helpers.h"
#endif
#ifdef USE_SOCKET_IPV4_ALLOW
#include "esphome/components/socket/ipv4_allow.h"
#endif
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "rtsp_session.h"

namespace esphome::rtsp_server {

#ifdef USE_RTSP_H264
class RTSPServer : public Component, public camera::CameraListener, public camera_video::H264StreamListener {
#else
class RTSPServer : public Component, public camera::CameraListener {
#endif
 public:
  explicit RTSPServer(uint16_t port) : port_(port) {}
#ifdef USE_RTSP_H264
  RTSPServer(uint16_t port, camera_video::H264Stream *stream) : h264_stream_(stream), port_(port) {}
  void set_rtp_payload_size(uint16_t payload_size) { this->h264_payload_size_ = payload_size; }
#endif
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override;

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
#ifdef USE_RTSP_H264
  void on_h264_frame(const camera_video::FrameRef<camera_video::H264Frame> &frame) override;
#endif
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const socket::Ipv4AllowEntry *entries, size_t count) { this->allow_.set(entries, count); }
#endif

 protected:
  void accept_new_connections_(uint32_t now);
  void socket_failed_(const char *what);
#ifdef USE_RTSP_H264
  void update_h264_metadata_();
  void update_h264_subscription_(bool playing);
  camera_video::FrameRef<camera_video::H264Frame> take_pending_h264_frame_();
  void clear_pending_h264_frame_();
#endif

  socket::ListenSocket *socket_{nullptr};
  std::array<RTSPSession, MAX_RTSP_CONNECTIONS> sessions_{};
#ifdef USE_SOCKET_IPV4_ALLOW
  socket::Ipv4Allow allow_;
#endif
#ifdef USE_RTSP_H264
  camera_video::H264Stream *h264_stream_{nullptr};
  Mutex h264_frame_mutex_;
  camera_video::FrameRef<camera_video::H264Frame> pending_h264_frame_;
  std::atomic<bool> h264_discontinuity_{false};
  uint16_t h264_payload_size_{1400};
  bool h264_subscribed_{false};
  bool h264_priming_{false};
  bool h264_ready_{false};
#endif
  uint16_t port_;
  bool streaming_{false};
  bool frame_warned_{false};
};

}  // namespace esphome::rtsp_server
#endif
