#pragma once

#ifdef USE_ESP32

#include <array>
#include <memory>

#include "esphome/components/camera/camera.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/string_ref.h"

namespace esphome::rtsp_server {

class RTSPServer;

/** A single RTSP client session: one TCP connection carrying both the RTSP
 *  control channel and, once playing, RTP/JPEG media interleaved on the same
 *  socket (RFC 2326 §10.12). No separate UDP transport is supported.
 */
class RTSPSession {
 public:
  RTSPSession(std::unique_ptr<socket::Socket> socket, RTSPServer *server, uint16_t server_port);

  /// Drives the request parser, response/RTP sender, and idle timeout. Call once per main loop tick.
  void loop();

  /// True once the session should be torn down and its slot reclaimed by the server.
  bool should_close() const { return this->should_close_; }

  /// Called by the server for every listener-side new frame while at least one session may be playing.
  /// Frames not requested for this session (not currently PLAYING) are ignored.
  void on_new_frame(const std::shared_ptr<camera::CameraImage> &image);

  /// Called by the server right before removing this session's slot, so any still-open
  /// stream-request accounting (start_stream/stop_stream bracketing) is released exactly once.
  void ensure_stream_stopped();

 protected:
  enum class State : uint8_t { INIT, READY, PLAYING };

  void read_requests_();
  void handle_request_(StringRef request);
  void handle_options_();
  void handle_describe_();
  void handle_setup_(StringRef transport_header);
  void handle_play_();
  void handle_teardown_();
  void handle_get_parameter_();
  void send_simple_response_(uint16_t code, const char *reason);

  void start_playing_();
  void stop_playing_();
  /// Returns any checked-out framebuffer to the camera pool. Must run on every
  /// transition that abandons an in-flight frame, or capture stalls device-wide.
  void drop_active_frame_();

  size_t build_next_jpeg_packet_();
  void flush_send_buffer_();

  std::unique_ptr<socket::Socket> socket_;
  RTSPServer *server_;
  std::unique_ptr<camera::CameraImageReader> image_reader_;

  State state_{State::INIT};
  bool should_close_{false};
  bool close_after_send_{false};
  bool playing_counted_{false};
  bool oversize_frame_warned_{false};

  // Inbound RTSP request accumulation (text protocol, terminated by a blank line).
  std::array<char, 512> request_buf_{};
  size_t request_len_{0};

  // Outbound buffer shared by RTSP text responses and interleaved RTP/JPEG packets --
  // only one is ever in flight at a time, which keeps socket byte ordering correct.
  static constexpr size_t RTP_JPEG_PAYLOAD_SIZE = 1024;
  static constexpr size_t SEND_BUFFER_SIZE = 4 /* interleave */ + 12 /* RTP */ + 8 /* JPEG main header */ +
                                             4 /* quant header */ + 128 /* 2 quant tables */ + RTP_JPEG_PAYLOAD_SIZE;
  std::array<uint8_t, SEND_BUFFER_SIZE> send_buf_{};
  size_t send_len_{0};
  size_t send_offset_{0};

  uint16_t server_port_;
  uint32_t cseq_{0};
  char session_id_[9]{};  // 8 hex digits + NUL

  // RTP state
  uint16_t sequence_number_{0};
  uint32_t rtp_timestamp_{0};
  uint32_t ssrc_{0};
  uint32_t last_activity_ms_{0};
  uint32_t last_frame_ms_{0};

  // Per-frame RTP/JPEG packetization state, populated from the JPEG's own markers
  // when a new frame starts sending (see rtsp_session.cpp for the parser).
  bool frame_active_{false};
  size_t frame_scan_len_{0};
  size_t frame_sent_{0};
  uint16_t jpeg_width_{0};
  uint16_t jpeg_height_{0};
  uint8_t jpeg_type_{0};
  const uint8_t *qtable0_{nullptr};
  size_t qtable0_len_{0};
  const uint8_t *qtable1_{nullptr};
  size_t qtable1_len_{0};
};

}  // namespace esphome::rtsp_server

#endif
