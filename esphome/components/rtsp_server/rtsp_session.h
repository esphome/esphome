#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <memory>

#include "esphome/components/camera_video/encoded_stream.h"
#include "rtp_packetizer.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/string_ref.h"

namespace esphome::rtsp_server {

static constexpr uint32_t FRAME_STALL_MS = 500;

/** A single RTSP client: one TCP connection carrying both the RTSP control channel and, once playing,
 *  RTP media interleaved on the same socket (RFC 2326 §10.12). No UDP transport is supported.
 *  The object is a reusable slot: open() starts a connection, and it closes itself when the connection ends.
 */
class RTSPSession {
 public:
  bool is_active() const { return this->socket_ != nullptr; }
  bool is_playing() const { return this->state_ == State::PLAYING; }

  void open(std::unique_ptr<socket::Socket> socket, uint32_t now, camera_video::EncodedVideoStream *stream,
            uint16_t payload_size);
  void loop(uint32_t now, uint16_t port);

  void on_encoded_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame, uint32_t now);
  void on_discontinuity();

 protected:
  enum class State : uint8_t { INIT, READY, PLAYING };
  void service_(uint32_t now, uint16_t port);
  void close_();
  void read_requests_(uint32_t now, uint16_t port);
  /// Consumes buffered input up to and including the next request; true once a request was handled.
  bool process_input_(uint16_t port);
  void consume_input_(size_t len);
  void handle_request_(StringRef request, uint16_t port);
  void handle_setup_(StringRef transport_header);
  void handle_play_();
  /// Queues "RTSP/1.0 <code> <reason>", CSeq, Session once set up, the extra headers and the blank line.
  void queue_response_(uint16_t code, const char *reason, const char *extra_headers_fmt = nullptr, ...)
      __attribute__((format(printf, 4, 5)));
  void stop_playing_();
  bool has_frame_() const;
  bool queue_next_packet_();
  /// Releases the frame; the frame-backed rest of the current packet is sent as zeros to keep the framing.
  void drop_frame_();
  /// Writes what is queued; true once all of it is written, false while backlogged or after a failure.
  bool flush_(uint32_t now);

  std::unique_ptr<socket::Socket> socket_;
  // Packetizer retains the encoded buffer through the last partial socket write.
  std::unique_ptr<RtpPacketizer> packetizer_;
  camera_video::EncodedVideoStream *stream_{nullptr};
  uint16_t payload_size_{1400};
  uint64_t timestamp_origin_us_{0};
  uint32_t timestamp_origin_rtp_{0};
  std::array<char, 768> sdp_buf_{};

  uint32_t cseq_{0};
  uint32_t rtp_timestamp_{0};
  uint32_t ssrc_{0};
  uint32_t last_activity_ms_{0};
  uint32_t last_frame_ms_{0};
  uint32_t frame_progress_ms_{0};
  // Bytes of an interleaved packet (RTCP) or request body from the client still to discard.
  uint32_t skip_len_{0};
  uint16_t sequence_number_{0};
  uint16_t request_len_{0};
  // Zeros still owed for a packet whose frame was dropped part way through.
  uint16_t filler_len_{0};

  // Pending output: a response or one RTP packet. Packets can point into retained frame storage.
  std::array<struct iovec, 4> iov_{};
  uint8_t iov_pos_{0};
  uint8_t iov_count_{0};

  State state_{State::INIT};
  bool should_close_{false};
  bool close_after_send_{false};
  bool sending_packet_{false};  // iov_ holds an RTP packet rather than a response
  bool read_pending_{false};    // reading stopped before EAGAIN; read again without waiting for ready()
  char session_id_[9]{};

  // Requests and interleaved packets from the client. A request is NUL terminated in place before it is
  // handled, for StringRef::find(), which uses strstr.
  std::array<char, 512> request_buf_{};
  // Response text, or the header of the RTP packet being sent.
  std::array<char, 256> send_buf_{};
};

}  // namespace esphome::rtsp_server

#endif
