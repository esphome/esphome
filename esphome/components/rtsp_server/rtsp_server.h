#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <memory>

// Sessions carry audio wherever USE_RTSP_SERVER_AUDIO is set, which the host unit tests use; capturing it
// needs the ESP32 ring buffer. Configurations set the define only on the ESP32.
#if defined(USE_RTSP_SERVER_AUDIO) && defined(USE_ESP32)
#define USE_RTSP_SERVER_CAPTURE
#endif

#include "esphome/components/camera/camera.h"
#ifdef USE_RTSP_SERVER_CAPTURE
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#endif
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
#ifdef USE_RTSP_SERVER_CAPTURE
  void set_microphone_source(microphone::MicrophoneSource *source) { this->microphone_source_ = source; }
  void set_audio_codec(AudioCodec codec) { this->audio_codec_ = codec; }
#endif

 protected:
  void accept_new_connections_(uint32_t now);
  void socket_failed_(const char *what);
#ifdef USE_RTSP_SERVER_CAPTURE
  /// Allocates the buffers and offers the audio track to the sessions; video keeps working if this fails.
  void setup_audio_();
  /// Moves whole packets of captured audio from the ring buffer into the track's slots.
  void read_audio_(uint32_t now);
#endif

  socket::ListenSocket *socket_{nullptr};
  std::array<RTSPSession, MAX_RTSP_CONNECTIONS> sessions_{};
#ifdef USE_SOCKET_IPV4_ALLOW
  socket::Ipv4Allow allow_;
#endif
#ifdef USE_RTSP_SERVER_CAPTURE
  microphone::MicrophoneSource *microphone_source_{nullptr};
  // Written by the microphone's task, read by the main loop.
  std::unique_ptr<ring_buffer::RingBuffer> ring_buffer_;
  AudioTrack audio_track_;
  std::array<char, 224> sdp_{};
  AudioCodec audio_codec_{AudioCodec::AUDIO_CODEC_L16};
  bool audio_ready_{false};
  bool capturing_{false};
#endif
  uint16_t port_;
  bool streaming_{false};
  bool frame_warned_{false};
};

}  // namespace esphome::rtsp_server

#endif
