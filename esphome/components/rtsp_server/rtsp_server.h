#pragma once

#ifdef USE_ESP32

#include <array>
#include <memory>

#include "esphome/components/camera/camera.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "rtsp_session.h"

namespace esphome::rtsp_server {

class RTSPServer : public Component, public camera::CameraListener {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_port(uint16_t port) { this->port_ = port; }

  /// camera::CameraListener
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;

  /// Called by a session when it transitions into/out of PLAYING, so the server can bracket
  /// camera::Camera::start_stream()/stop_stream() around the set of currently-playing sessions.
  void session_started_playing();
  void session_stopped_playing();

 protected:
  void accept_new_connections_();
  void socket_failed_(const char *what);

  socket::ListenSocket *socket_{nullptr};
  uint16_t port_{554};
  uint8_t playing_count_{0};

  std::array<std::unique_ptr<RTSPSession>, MAX_RTSP_SESSIONS> sessions_{};
  uint8_t session_count_{0};
};

}  // namespace esphome::rtsp_server

#endif
