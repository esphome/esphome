#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <memory>

#include "esphome/components/camera/camera.h"
#ifdef USE_SOCKET_IPV4_ALLOW
#include "esphome/components/socket/ipv4_allow.h"
#endif
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "rtsp_session.h"

namespace esphome::rtsp_server {

class RTSPServer : public Component, public camera::CameraListener {
 public:
  explicit RTSPServer(uint16_t port) : port_(port) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const socket::Ipv4AllowEntry *entries, size_t count) { this->allow_.set(entries, count); }
#endif

 protected:
  void accept_new_connections_(uint32_t now);
  void socket_failed_(const char *what);

  socket::ListenSocket *socket_{nullptr};
  std::array<RTSPSession, MAX_RTSP_CONNECTIONS> sessions_{};
#ifdef USE_SOCKET_IPV4_ALLOW
  socket::Ipv4Allow allow_;
#endif
  uint16_t port_;
  bool streaming_{false};
  bool frame_warned_{false};
};

}  // namespace esphome::rtsp_server

#endif
