#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA

#include <array>
#include <memory>

#include "esphome/components/camera/camera.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/log.h"
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
static constexpr size_t FILL_LEN = 64;  // padding is written in chunks of this size
// Interleaved channels (RFC 2326 section 10.12), fixed per track.
static constexpr uint8_t VIDEO_RTP_CHANNEL = 0;
static constexpr uint8_t VIDEO_RTCP_CHANNEL = 1;
static constexpr uint8_t AUDIO_RTP_CHANNEL = 2;
static constexpr uint8_t AUDIO_RTCP_CHANNEL = 3;

#ifdef USE_RTSP_SERVER_AUDIO
// Audio packets kept for the sessions; a session that falls further behind skips the oldest.
static constexpr size_t AUDIO_SLOTS = 8;
static constexpr uint32_t AUDIO_PACKET_MS = 20;
static constexpr size_t RTCP_SR_LEN = 28;
static constexpr size_t RTCP_CNAME_LEN = 8;  // the session id
// SDES header, SSRC, CNAME item (type, length, text), END item, padded to 32 bits (RFC 3550 section 6.5)
static constexpr size_t RTCP_SDES_LEN = 4 + ((4 + 2 + RTCP_CNAME_LEN + 1 + 3) & ~size_t{3});
// A sender report and the SDES with the CNAME, the compound packet RTP/AVP requires (RFC 3550 section 6.1).
static constexpr size_t RTCP_REPORT_LEN = RTCP_SR_LEN + RTCP_SDES_LEN;
// Sender reports let receivers line up the audio and video timestamps (RFC 3550 section 6.4.1).
static constexpr uint32_t RTCP_INTERVAL_MS = 5000;

enum class AudioCodec : uint8_t { AUDIO_CODEC_L16, AUDIO_CODEC_PCMU, AUDIO_CODEC_PCMA };

/// Encoded audio packets shared by all sessions, written by the server from the main loop.
/// Packet n lives in slot n % AUDIO_SLOTS until packet n + AUDIO_SLOTS replaces it.
struct AudioTrack {
  const uint8_t *packet(uint32_t index) const { return this->slots + (index % AUDIO_SLOTS) * this->payload_len; }

  uint8_t *slots{nullptr};
  const char *sdp{nullptr};  // the whole SDP body, video and audio
  // Time (millis_64()) the first sample of each packet was captured, for the sender reports.
  std::array<uint64_t, AUDIO_SLOTS> capture_ms{};
  // Position of each packet's first sample in the captured stream; samples the device lost leave a gap.
  std::array<uint32_t, AUDIO_SLOTS> sample_pos{};
  uint32_t produced{0};  // packets written so far
  uint16_t sdp_len{0};
  uint16_t payload_len{0};
  uint16_t samples_per_packet{0};
  uint8_t payload_type{0};
  const uint8_t *filler{nullptr};  // FILL_LEN bytes of the codec's silence
};

/// The codec's encoding name, as in the SDP rtpmap.
const LogString *audio_codec_name(AudioCodec codec);
/// Writes the SDP body with the audio media section; returns its length, or 0 if it does not fit.
size_t format_audio_sdp(char *buf, size_t size, AudioCodec codec, uint8_t payload_type, uint32_t sample_rate);
/// FILL_LEN bytes of the codec's silence, to pad a packet cut loose from its slot (zeros decode as loud clicks
/// in G.711).
const uint8_t *silence_fill(AudioCodec codec);
uint8_t encode_ulaw(int16_t sample);
uint8_t encode_alaw(int16_t sample);
#endif

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
 *  RTP/JPEG media (and audio, when a track is set) interleaved on the same socket (RFC 2326 §10.12).
 *  No UDP transport is supported.
 *  The object is a reusable slot: open() starts a connection, and it closes itself when the connection ends.
 */
class RTSPSession {
 public:
  bool is_active() const { return this->socket_ != nullptr; }
  bool is_playing() const { return this->state_ == State::PLAYING; }
  bool is_playing_video() const { return this->is_playing() && (this->tracks_ & TRACK_VIDEO); }
#ifdef USE_RTSP_SERVER_AUDIO
  bool is_playing_audio() const { return this->is_playing() && (this->tracks_ & TRACK_AUDIO); }
  /// Set once before the first connection; without a track, only the video stream is offered.
  void set_audio_track(const AudioTrack *track) { this->audio_track_ = track; }
  /// Called before the server replaces the slot of packet `index`; if that packet is being written, the
  /// rest of it is sent as the codec's silence (AudioTrack::filler) to keep the framing.
  void on_audio_slot_reuse(uint32_t index);
#endif

  void open(std::unique_ptr<socket::Socket> socket, uint32_t now);
  void loop(uint32_t now, uint16_t port);
  /// Ignored unless playing with no output pending; packets of the frame being sent point into that frame.
  /// `time_ms` is the frame's time on the millis_64() clock, for the sender reports.
  void on_new_frame(const std::shared_ptr<camera::CameraImage> &image, const JpegInfo &jpeg, uint32_t now,
                    uint64_t time_ms);

 protected:
  enum class State : uint8_t { INIT, READY, PLAYING };
  // What iov_ holds: a response (or a packet already cut loose from its frame), a video packet pointing into
  // frame_, an audio packet pointing into a track slot, or sender reports. A new frame can follow the last two.
  enum class Output : uint8_t { OUTPUT_CONTROL, OUTPUT_FRAME, OUTPUT_AUDIO, OUTPUT_REPORT };
  static constexpr uint8_t TRACK_VIDEO = 1 << 0;
  static constexpr uint8_t TRACK_AUDIO = 1 << 1;

  void service_(uint32_t now, uint16_t port);
  void close_();
  void read_requests_(uint32_t now, uint16_t port);
  /// Consumes buffered input up to and including the next request; true once a request was handled.
  bool process_input_(uint16_t port);
  void consume_input_(size_t len);
  void handle_request_(StringRef request, uint16_t port);
  void handle_setup_(StringRef uri, StringRef transport_header);
  void handle_play_();
  /// Queues "RTSP/1.0 <code> <reason>", CSeq, Session once set up, the extra headers and the blank line.
  void queue_response_(uint16_t code, const char *reason, const char *extra_headers_fmt = nullptr, ...)
      __attribute__((format(printf, 4, 5)));
  void stop_playing_();
  bool queue_next_packet_(uint32_t now);
  bool queue_video_packet_();
#ifdef USE_RTSP_SERVER_AUDIO
  bool queue_audio_packet_();
  bool queue_sender_reports_(uint32_t now);
#endif
  /// Releases the frame; the frame-backed rest of the current packet is sent as zeros to keep the framing.
  void drop_frame_();
  /// Replaces the entries after the packet header with zeros, so the packet no longer points into shared data.
  void detach_payload_();
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
#ifdef USE_RTSP_SERVER_AUDIO
  uint64_t frame_time_ms_{0};  // of the last frame taken, for the sender reports
#endif
  // Bytes of an interleaved packet (RTCP) or request body from the client still to discard.
  uint32_t skip_len_{0};
  uint16_t sequence_number_{0};
  uint16_t request_len_{0};
  // Filler still owed for a packet cut loose from its frame or slot part way through.
  uint16_t filler_len_{0};
  const uint8_t *filler_{nullptr};  // 64 bytes of zeros, or of encoded audio silence

#ifdef USE_RTSP_SERVER_AUDIO
  const AudioTrack *audio_track_{nullptr};
  uint32_t audio_next_{0};       // the track's index of the next packet to send
  uint32_t audio_in_flight_{0};  // the packet iov_ holds while output_ is OUTPUT_AUDIO
  uint32_t audio_ssrc_{0};
  uint32_t audio_rtp_offset_{0};
  uint32_t audio_timestamp_{0};  // of the last packet sent
  uint64_t audio_capture_ms_{0};
  uint32_t audio_packets_{0};
  uint32_t audio_octets_{0};
  uint32_t video_packets_{0};
  uint32_t video_octets_{0};
  uint32_t last_report_ms_{0};
  uint16_t audio_sequence_number_{0};
  bool video_turn_{false};  // a video packet goes next, so audio cannot starve the video
#endif

  // Pending output: a response or one RTP packet. Packets point at the frame's tables and scan data.
  std::array<struct iovec, 4> iov_{};
  uint8_t iov_pos_{0};
  uint8_t iov_count_{0};

  State state_{State::INIT};
  Output output_{Output::OUTPUT_CONTROL};
  uint8_t tracks_{0};  // TRACK_ bits set up by the client
  bool should_close_{false};
  bool close_after_send_{false};
  bool read_pending_{false};  // reading stopped before EAGAIN; read again without waiting for ready()
  char session_id_[9]{};

  // Requests and interleaved packets from the client. A request is NUL terminated in place before it is
  // handled, for StringRef::find(), which uses strstr.
  std::array<char, 512> request_buf_{};
  // Response text, or the header of the RTP packet being sent.
  std::array<char, 256> send_buf_{};
};

}  // namespace esphome::rtsp_server

#endif
