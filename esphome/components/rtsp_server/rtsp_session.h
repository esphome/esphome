#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <memory>

#include "esphome/components/camera/camera.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/string_ref.h"

namespace esphome::rtsp_server {

static constexpr size_t INTERLEAVED_HEADER_LEN = 4;
static constexpr size_t RTP_HEADER_LEN = 12;
static constexpr size_t JPEG_HEADER_LEN = 8;
static constexpr size_t QUANT_HEADER_LEN = 4;
static constexpr size_t QTABLE_LEN = 64;  // one 8-bit precision quantization table
static constexpr size_t RTP_JPEG_PAYLOAD_SIZE = 1024;
static constexpr size_t MAX_PACKET_SIZE = INTERLEAVED_HEADER_LEN + RTP_HEADER_LEN + JPEG_HEADER_LEN + QUANT_HEADER_LEN +
                                          2 * QTABLE_LEN + RTP_JPEG_PAYLOAD_SIZE;
// RFC 2435 stores width and height in units of 8 pixels in one byte each.
static constexpr uint16_t RTP_JPEG_MAX_DIMENSION = 255 * 8;
// A frame whose packets make no write progress for this long is dropped, so a stalled client cannot hold
// the camera image (esp32_camera captures nothing while any copy is held).
static constexpr uint32_t FRAME_STALL_MS = 500;

/// The fields of a JPEG frame that an RFC 2435 RTP/JPEG payload needs. Pointers refer into the frame.
struct JpegInfo {
  const uint8_t *qtable0{nullptr};
  const uint8_t *qtable1{nullptr};  // same as qtable0 when the frame has one table
  const uint8_t *scan_data{nullptr};
  size_t scan_len{0};  // entropy-coded data, without the trailing EOI marker
  uint16_t width{0};
  uint16_t height{0};
  uint8_t type{0};  // RFC 2435 type: 0 = 4:2:2 subsampling, 1 = 4:2:0
};

bool parse_jpeg_info(const uint8_t *data, size_t len, JpegInfo &out);

/** A single RTSP client: one TCP connection carrying both the RTSP control channel and, once playing,
 *  RTP/JPEG media interleaved on the same socket (RFC 2326 §10.12). No UDP transport is supported.
 *  The object is a reusable slot: open() starts a connection, and it closes itself when the connection ends.
 */
class RTSPSession {
 public:
  bool is_active() const { return this->socket_ != nullptr; }
  bool is_playing() const { return this->state_ == State::PLAYING; }

  void open(std::unique_ptr<socket::Socket> socket, uint32_t now);
  void loop(uint32_t now, uint16_t port);
  /// Ignored unless playing with no output pending; packets of the frame being sent point into that frame.
  void on_new_frame(const std::shared_ptr<camera::CameraImage> &image, const JpegInfo &jpeg, uint32_t now);

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
  bool queue_next_packet_();
  /// Releases the frame; the frame-backed rest of the current packet is sent as zeros to keep the framing.
  void drop_frame_();
  /// Writes what is queued; true once all of it is written, false while backlogged or after a failure.
  bool flush_(uint32_t now);

  std::unique_ptr<socket::Socket> socket_;
  // Held until its last packet is written; esp32_camera captures nothing new while any copy is held.
  std::shared_ptr<camera::CameraImage> frame_;
  JpegInfo jpeg_;
  size_t frame_sent_{0};

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

  // Pending output: a response or one RTP packet. Packets point at the frame's tables and scan data.
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
