#include "rtsp_session.h"

#ifdef USE_CAMERA

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

static constexpr uint8_t RTP_PAYLOAD_TYPE_JPEG = 26;
static constexpr uint32_t RTP_CLOCK_RATE = 90000;
static constexpr uint32_t SESSION_TIMEOUT_MS = 60000;
static constexpr char REQUEST_END[] = "\r\n\r\n";
static constexpr size_t REQUEST_END_LEN = sizeof(REQUEST_END) - 1;
static constexpr uint8_t ZERO_FILL[FILL_LEN]{};
static constexpr char SDP_BODY[] = "v=0\r\n"
                                   "o=- 0 0 IN IP4 0.0.0.0\r\n"
                                   "s=ESPHome RTSP\r\n"
                                   "t=0 0\r\n"
                                   "m=video 0 RTP/AVP 26\r\n"
                                   "a=control:trackID=0\r\n";
#ifdef USE_RTSP_SERVER_AUDIO
static constexpr uint8_t RTCP_SENDER_REPORT = 200;
static constexpr uint8_t RTCP_SOURCE_DESCRIPTION = 202;
static constexpr uint8_t RTCP_SDES_CNAME = 1;

static constexpr std::array<uint8_t, FILL_LEN> make_fill(uint8_t value) {
  std::array<uint8_t, FILL_LEN> fill{};
  for (auto &byte : fill)
    byte = value;
  return fill;
}
// G.711 silence: encode_ulaw(0) and encode_alaw(0).
static constexpr std::array<uint8_t, FILL_LEN> ULAW_SILENCE_FILL = make_fill(0xFF);
static constexpr std::array<uint8_t, FILL_LEN> ALAW_SILENCE_FILL = make_fill(0xD5);
#endif

namespace {

StringRef trim(const StringRef &s) {
  const char *start = s.c_str();
  size_t len = s.size();
  while (len > 0 && (*start == ' ' || *start == '\t')) {
    start++;
    len--;
  }
  while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\r')) {
    len--;
  }
  return {start, len};
}

const char *find_request_end(const char *buf, size_t len) {
  for (size_t i = 0; i + REQUEST_END_LEN <= len; i++) {
    if (std::memcmp(buf + i, REQUEST_END, REQUEST_END_LEN) == 0)
      return buf + i;
  }
  return nullptr;
}

/// Bounded substring search; StringRef::find() uses strstr and could run past the header value.
bool contains(const StringRef &hay, const StringRef &needle) {
  if (needle.size() > hay.size())
    return false;
  for (size_t i = 0; i + needle.size() <= hay.size(); i++) {
    if (std::memcmp(hay.c_str() + i, needle.c_str(), needle.size()) == 0)
      return true;
  }
  return false;
}

/// Writes the 12-byte RTP header and returns the pointer past it.
uint8_t *put_rtp_header(uint8_t *buf, bool marker, uint8_t payload_type, uint16_t sequence, uint32_t timestamp,
                        uint32_t ssrc) {
  *buf++ = 0x80;  // V=2, P=0, X=0, CC=0
  *buf++ = static_cast<uint8_t>((marker ? 0x80 : 0x00) | payload_type);
  buf = put_be16(buf, sequence);
  buf = put_be32(buf, timestamp);
  return put_be32(buf, ssrc);
}

#ifdef USE_RTSP_SERVER_AUDIO
/// Writes an interleaved compound RTCP packet, a sender report (RFC 3550 section 6.4.1) and an SDES with the
/// CNAME, and returns the pointer past it. The NTP timestamp counts from boot; receivers only compare the reports
/// of one session with each other, which share the CNAME.
uint8_t *put_sender_report(uint8_t *buf, uint8_t channel, uint32_t ssrc, uint64_t time_ms, uint32_t rtp_timestamp,
                           uint32_t packets, uint32_t octets, const char *cname) {
  *buf++ = '$';
  *buf++ = channel;
  buf = put_be16(buf, RTCP_REPORT_LEN);
  *buf++ = 0x80;  // V=2, P=0, RC=0
  *buf++ = RTCP_SENDER_REPORT;
  buf = put_be16(buf, RTCP_SR_LEN / 4 - 1);
  buf = put_be32(buf, ssrc);
  buf = put_be32(buf, static_cast<uint32_t>(time_ms / 1000));
  buf = put_be32(buf, static_cast<uint32_t>(((time_ms % 1000) << 32) / 1000));
  buf = put_be32(buf, rtp_timestamp);
  buf = put_be32(buf, packets);
  buf = put_be32(buf, octets);

  uint8_t *const sdes = buf;
  *buf++ = 0x81;  // V=2, P=0, SC=1
  *buf++ = RTCP_SOURCE_DESCRIPTION;
  buf = put_be16(buf, RTCP_SDES_LEN / 4 - 1);
  buf = put_be32(buf, ssrc);
  *buf++ = RTCP_SDES_CNAME;
  *buf++ = RTCP_CNAME_LEN;
  std::memcpy(buf, cname, RTCP_CNAME_LEN);
  buf += RTCP_CNAME_LEN;
  while (buf < sdes + RTCP_SDES_LEN)
    *buf++ = 0;  // END item and padding
  return buf;
}
#endif

}  // namespace

#ifdef USE_RTSP_SERVER_AUDIO
// In AudioCodec order
PROGMEM_STRING_TABLE(AudioCodecNames, "L16", "PCMU", "PCMA");

const LogString *audio_codec_name(AudioCodec codec) {
  return AudioCodecNames::get_log_str(static_cast<uint8_t>(codec), 0);
}

const uint8_t *silence_fill(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::AUDIO_CODEC_PCMU:
      return ULAW_SILENCE_FILL.data();
    case AudioCodec::AUDIO_CODEC_PCMA:
      return ALAW_SILENCE_FILL.data();
    default:
      return ZERO_FILL;
  }
}

size_t format_audio_sdp(char *buf, size_t size, AudioCodec codec, uint8_t payload_type, uint32_t sample_rate) {
  int len = snprintf(buf, size,
                     "%s"
                     "m=audio 0 RTP/AVP %u\r\n"
                     "a=rtpmap:%u %s/%" PRIu32 "/1\r\n"
                     "a=control:trackID=1\r\n",
                     SDP_BODY, payload_type, payload_type, LOG_STR_ARG(audio_codec_name(codec)), sample_rate);
  return len > 0 && static_cast<size_t>(len) < size ? static_cast<size_t>(len) : 0;
}

// G.711 encoders as in the ITU-T reference (and the Sun g711.c most implementations derive from).
uint8_t encode_ulaw(int16_t sample) {
  static constexpr int32_t ULAW_BIAS = 0x84;
  static constexpr int32_t ULAW_CLIP = 32635;
  int32_t value = sample;
  uint8_t sign = 0;
  if (value < 0) {
    value = -value;
    sign = 0x80;
  }
  value = std::min(value, ULAW_CLIP) + ULAW_BIAS;
  uint8_t exponent = 7;
  for (int32_t mask = 0x4000; (value & mask) == 0 && exponent > 0; mask >>= 1)
    exponent--;
  const auto mantissa = static_cast<uint8_t>((value >> (exponent + 3)) & 0x0F);
  return static_cast<uint8_t>(~(sign | (exponent << 4) | mantissa));
}

uint8_t encode_alaw(int16_t sample) {
  int32_t value = sample >> 3;  // 13-bit magnitude
  uint8_t mask = 0xD5;
  if (value < 0) {
    mask = 0x55;
    value = -value - 1;
  }
  uint8_t segment = 0;
  while (segment < 8 && value >= (0x20 << segment))  // segment ends at (0x20 << segment) - 1
    segment++;
  if (segment >= 8)
    return 0x7F ^ mask;
  const int32_t mantissa = (segment < 2 ? value >> 1 : value >> segment) & 0x0F;
  return static_cast<uint8_t>(((segment << 4) | mantissa) ^ mask);
}
#endif

/// Scans JPEG marker segments (SOF0/1, DQT, SOS) for the dimensions, quantization tables and the start of
/// the entropy-coded scan data. Assumes the standard Huffman tables (true for esp32-camera), so DHT segments
/// are not parsed; receivers rebuild them per RFC 2435. Only three-component 4:2:2 or 4:2:0 frames without
/// restart markers (DRI) can be carried as RFC 2435 types 0 and 1; anything else returns false.
bool parse_jpeg_info(const uint8_t *data, size_t len, JpegInfo &out) {
  if (len < 4 || data[0] != 0xFF || data[1] != 0xD8)
    return false;  // no SOI

  size_t pos = 2;
  while (pos + 2 <= len) {
    if (data[pos] != 0xFF) {
      pos++;  // resync on a stray non-marker byte
      continue;
    }
    uint8_t marker = data[pos + 1];
    if (marker == 0xFF) {
      pos++;  // fill byte before the real marker
      continue;
    }
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD9)) {
      pos += 2;  // standalone marker, no length field (TEM, RSTn, SOI, EOI)
      continue;
    }
    if (pos + 4 > len)
      break;

    uint16_t seg_len = encode_uint16(data[pos + 2], data[pos + 3]);
    size_t seg_start = pos + 4;
    if (seg_len < 2 || seg_start + (seg_len - 2) > len)
      return false;
    size_t seg_end = seg_start + (seg_len - 2);

    if (marker == 0xC0 || marker == 0xC1) {  // SOF0 / SOF1
      if (seg_len < 8 + 3 * 3 || data[seg_start + 5] != 3)
        return false;  // not three components
      out.height = encode_uint16(data[seg_start + 1], data[seg_start + 2]);
      out.width = encode_uint16(data[seg_start + 3], data[seg_start + 4]);
      const uint8_t sampling = data[seg_start + 7];  // Y component H/V sampling factors
      if (sampling == 0x21) {
        out.type = 0;  // 4:2:2
      } else if (sampling == 0x22) {
        out.type = 1;  // 4:2:0
      } else {
        return false;
      }
    } else if (marker == 0xDD) {  // DRI
      return false;
    } else if (marker == 0xDB) {  // DQT, one or more tables back to back
      size_t p = seg_start;
      while (p < seg_end) {
        uint8_t precision = data[p] >> 4;
        uint8_t table_id = data[p] & 0x0F;
        size_t table_len = precision == 0 ? QTABLE_LEN : 2 * QTABLE_LEN;
        if (p + 1 + table_len > seg_end)
          break;
        if (precision == 0 && table_id == 0) {
          out.qtable0 = &data[p + 1];
        } else if (precision == 0 && table_id == 1) {
          out.qtable1 = &data[p + 1];
        }
        p += 1 + table_len;
      }
    } else if (marker == 0xDA) {  // SOS, scan data follows
      if (out.width == 0 || out.height == 0 || out.qtable0 == nullptr)
        return false;
      if (out.qtable1 == nullptr)
        out.qtable1 = out.qtable0;
      out.scan_data = data + seg_end;
      out.scan_len = len - seg_end;
      if (out.scan_len >= 2 && out.scan_data[out.scan_len - 2] == 0xFF && out.scan_data[out.scan_len - 1] == 0xD9)
        out.scan_len -= 2;  // the RFC 2435 payload is the entropy-coded data only
      return true;
    }

    pos = seg_end;
  }
  return false;
}

void RTSPSession::open(std::unique_ptr<socket::Socket> socket, uint32_t now) {
  this->socket_ = std::move(socket);
  this->socket_->setblocking(false);
  int enable = 1;
  this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
  this->should_close_ = false;
  this->close_after_send_ = false;
  this->read_pending_ = false;
  this->request_len_ = 0;
  this->skip_len_ = 0;
  this->filler_len_ = 0;
  this->cseq_ = 0;
  this->last_frame_ms_ = 0;
  this->last_activity_ms_ = now;
  this->ssrc_ = random_uint32();
  this->sequence_number_ = static_cast<uint16_t>(random_uint32());
  this->rtp_timestamp_ = random_uint32();
  this->tracks_ = 0;
  this->filler_ = ZERO_FILL;
#ifdef USE_RTSP_SERVER_AUDIO
  this->audio_ssrc_ = random_uint32();
  this->audio_sequence_number_ = static_cast<uint16_t>(random_uint32());
  this->audio_rtp_offset_ = random_uint32();
  this->audio_packets_ = 0;
  this->audio_octets_ = 0;
  this->video_packets_ = 0;
  this->video_octets_ = 0;
#endif
  format_hex_to(this->session_id_, random_uint32());
}

void RTSPSession::close_() {
  this->frame_.reset();
  this->socket_.reset();
  this->state_ = State::INIT;
  this->iov_pos_ = 0;
  this->iov_count_ = 0;
  this->filler_len_ = 0;
}

void RTSPSession::loop(uint32_t now, uint16_t port) {
  if (this->socket_ == nullptr)
    return;
  if (now - this->last_activity_ms_ > SESSION_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Session %s timed out", this->session_id_);
    this->should_close_ = true;
  } else {
    this->service_(now, port);
  }
  if (this->should_close_)
    this->close_();
}

void RTSPSession::service_(uint32_t now, uint16_t port) {
  if (this->frame_ != nullptr && now - this->frame_progress_ms_ > FRAME_STALL_MS) {
    ESP_LOGV(TAG, "Session %s stalled, dropping frame", this->session_id_);
    this->drop_frame_();
  }
  if (!this->flush_(now))
    return;
  if (!this->close_after_send_)
    this->read_requests_(now, port);
  // Drain the current frame until done or the socket backs up; one packet per loop would cap the frame rate.
  while (this->flush_(now)) {
    if (this->close_after_send_) {
      this->should_close_ = true;
      return;
    }
    if (!this->queue_next_packet_(now))
      return;
  }
}

void RTSPSession::read_requests_(uint32_t now, uint16_t port) {
  // A pipelined request may already be buffered; it is handled once the previous response is written.
  if (this->process_input_(port))
    return;
  if (!this->read_pending_ && !this->socket_->ready())
    return;
  this->read_pending_ = false;

  while (true) {
    if (this->request_len_ >= this->request_buf_.size()) {
      ESP_LOGW(TAG, "Request too large, closing session");
      this->should_close_ = true;
      return;
    }
    ssize_t n = this->socket_->read(this->request_buf_.data() + this->request_len_,
                                    this->request_buf_.size() - this->request_len_);
    if (n > 0) {
      this->request_len_ += static_cast<uint16_t>(n);
      this->last_activity_ms_ = now;
      if (this->process_input_(port)) {
        this->read_pending_ = true;  // stopped before EAGAIN, so ready() may not report what is left
        return;
      }
      continue;
    }
    if (n == 0) {
      this->should_close_ = true;
      return;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "Read failed: errno %d", errno);
      this->should_close_ = true;
    }
    return;
  }
}

bool RTSPSession::process_input_(uint16_t port) {
  while (this->request_len_ > 0) {
    if (this->skip_len_ > 0) {
      const size_t len = std::min<size_t>(this->skip_len_, this->request_len_);
      this->consume_input_(len);
      this->skip_len_ -= len;
      continue;
    }
    char *buf = this->request_buf_.data();
    if (buf[0] == '$') {  // interleaved packet: '$', channel, 16-bit length, payload
      if (this->request_len_ < INTERLEAVED_HEADER_LEN)
        return false;
      this->skip_len_ =
          INTERLEAVED_HEADER_LEN + encode_uint16(static_cast<uint8_t>(buf[2]), static_cast<uint8_t>(buf[3]));
      continue;
    }
    const char *end = find_request_end(buf, this->request_len_);
    if (end == nullptr)
      return false;  // wait for the rest of the request
    const size_t len = end - buf;
    buf[len] = '\0';  // over the terminator's first CR
    this->handle_request_(StringRef(buf, len), port);
    this->consume_input_(len + REQUEST_END_LEN);
    return true;
  }
  return false;
}

void RTSPSession::consume_input_(size_t len) {
  this->request_len_ -= len;
  std::memmove(this->request_buf_.data(), this->request_buf_.data() + len, this->request_len_);
}

void RTSPSession::handle_request_(StringRef request, uint16_t port) {
  size_t line_end = request.find("\r\n");
  StringRef first_line = line_end == std::string::npos ? request : StringRef(request.c_str(), line_end);

  this->cseq_ = 0;
  StringRef transport;
  int64_t content_length = 0;

  // Headers first: even a 400 reply has to echo the request's CSeq
  size_t pos = line_end == std::string::npos ? request.size() : line_end + 2;
  while (pos < request.size()) {
    size_t next = request.find("\r\n", pos);
    size_t header_end = next == std::string::npos ? request.size() : next;
    StringRef header(request.c_str() + pos, header_end - pos);

    size_t colon = header.find(':');
    if (colon != std::string::npos) {
      StringRef key = trim(StringRef(header.c_str(), colon));
      StringRef value = trim(StringRef(header.c_str() + colon + 1, header.size() - colon - 1));
      if (str_equals_case_insensitive(key, StringRef::from_lit("CSeq"))) {
        this->cseq_ = static_cast<uint32_t>(stol(value));
      } else if (str_equals_case_insensitive(key, StringRef::from_lit("Transport"))) {
        transport = value;
      } else if (str_equals_case_insensitive(key, StringRef::from_lit("Content-Length"))) {
        content_length = stol(value);
      }
    }

    if (next == std::string::npos)
      break;
    pos = next + 2;
  }

  if (content_length < 0 || content_length > static_cast<int64_t>(this->request_buf_.size())) {
    this->queue_response_(400, "Bad Request");
    this->close_after_send_ = true;
    return;
  }
  // The body is ignored; it is discarded as it arrives, like an interleaved packet.
  this->skip_len_ = static_cast<uint32_t>(content_length);

  size_t sp1 = first_line.find(' ');
  size_t sp2 = sp1 == std::string::npos ? std::string::npos : first_line.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) {
    this->queue_response_(400, "Bad Request");
    return;
  }
  StringRef method(first_line.c_str(), sp1);

  if (str_equals_case_insensitive(method, StringRef::from_lit("OPTIONS"))) {
    this->queue_response_(200, "OK", "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER\r\n");
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("DESCRIBE"))) {
    char addr_buf[socket::SOCKADDR_STR_LEN];
    this->socket_->getsockname_to(addr_buf);
    const bool ipv6 = std::strchr(addr_buf, ':') != nullptr;
    const char *sdp = SDP_BODY;
    size_t sdp_len = sizeof(SDP_BODY) - 1;
#ifdef USE_RTSP_SERVER_AUDIO
    if (this->audio_track_ != nullptr) {
      sdp = this->audio_track_->sdp;
      sdp_len = this->audio_track_->sdp_len;
    }
#endif
    this->queue_response_(200, "OK",
                          "Content-Base: rtsp://%s%s%s:%u/\r\n"
                          "Content-Type: application/sdp\r\n"
                          "Content-Length: %u\r\n",
                          ipv6 ? "[" : "", addr_buf, ipv6 ? "]" : "", port, static_cast<unsigned>(sdp_len));
    this->iov_[this->iov_count_++] = {const_cast<char *>(sdp), sdp_len};
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("SETUP"))) {
    this->handle_setup_(StringRef(first_line.c_str() + sp1 + 1, sp2 - sp1 - 1), transport);
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("PLAY"))) {
    this->handle_play_();
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("TEARDOWN"))) {
    this->stop_playing_();
    this->queue_response_(200, "OK");
    this->close_after_send_ = true;
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("GET_PARAMETER"))) {
    this->queue_response_(200, "OK");
  } else {
    this->queue_response_(501, "Not Implemented");
  }
}

void RTSPSession::queue_response_(uint16_t code, const char *reason, const char *extra_headers_fmt, ...) {
  char *buf = this->send_buf_.data();
  const size_t size = this->send_buf_.size();
  size_t pos = buf_append_printf(buf, size, 0, "RTSP/1.0 %u %s\r\nCSeq: %" PRIu32 "\r\n", code, reason, this->cseq_);
  if (this->state_ != State::INIT)
    pos = buf_append_printf(buf, size, pos, "Session: %s;timeout=60\r\n", this->session_id_);
  if (extra_headers_fmt != nullptr && pos < size) {
    va_list args;
    va_start(args, extra_headers_fmt);
    int written = vsnprintf(buf + pos, size - pos, extra_headers_fmt, args);
    va_end(args);
    if (written > 0)
      pos = std::min(pos + static_cast<size_t>(written), size);
  }
  pos = buf_append_str(buf, size, pos, "\r\n");
  this->iov_[0] = {buf, std::min(pos, size - 1)};
  this->iov_pos_ = 0;
  this->iov_count_ = 1;
  this->output_ = Output::OUTPUT_CONTROL;
}

void RTSPSession::handle_setup_([[maybe_unused]] StringRef uri, StringRef transport_header) {
  bool audio = false;
#ifdef USE_RTSP_SERVER_AUDIO
  // The SDP names the tracks trackID=0 (video) and trackID=1 (audio); any other URL is the video track.
  audio = contains(uri, StringRef::from_lit("trackID=1"));
  if (audio && this->audio_track_ == nullptr) {
    this->queue_response_(404, "Not Found");
    return;
  }
#endif
  if (!contains(transport_header, StringRef::from_lit("RTP/AVP/TCP"))) {
    this->queue_response_(461, "Unsupported Transport");
    return;
  }
  // A SETUP while streaming stops playback, ending in READY like a first SETUP.
  this->stop_playing_();
  // Each track uses fixed interleaved channels, whatever the client asked for.
  if (audio) {
    this->tracks_ |= TRACK_AUDIO;
    this->queue_response_(200, "OK", "Transport: RTP/AVP/TCP;unicast;interleaved=%u-%u\r\n", AUDIO_RTP_CHANNEL,
                          AUDIO_RTCP_CHANNEL);
  } else {
    this->tracks_ |= TRACK_VIDEO;
    this->queue_response_(200, "OK", "Transport: RTP/AVP/TCP;unicast;interleaved=%u-%u\r\n", VIDEO_RTP_CHANNEL,
                          VIDEO_RTCP_CHANNEL);
  }
}

void RTSPSession::handle_play_() {
  if (this->state_ == State::INIT) {
    this->queue_response_(455, "Method Not Valid In This State");
    return;
  }
  this->state_ = State::PLAYING;
  this->frame_.reset();  // a repeated PLAY may arrive mid-frame
#ifdef USE_RTSP_SERVER_AUDIO
  if (this->audio_track_ != nullptr)
    this->audio_next_ = this->audio_track_->produced;  // start with fresh audio
  this->last_report_ms_ = 0;                           // report as soon as both tracks have sent
#endif
  this->queue_response_(200, "OK", "Range: npt=0.000-\r\n");
}

void RTSPSession::stop_playing_() {
  this->state_ = State::READY;
  this->frame_.reset();
}

void RTSPSession::on_new_frame(const std::shared_ptr<camera::CameraImage> &image, const JpegInfo &jpeg, uint32_t now,
                               [[maybe_unused]] uint64_t time_ms) {
  // Pending output other than audio means a frame is in flight or a stalled one is still being padded out.
  if (!this->is_playing_video() || this->frame_ != nullptr || this->filler_len_ > 0 ||
      (this->iov_pos_ < this->iov_count_ && this->output_ != Output::OUTPUT_AUDIO &&
       this->output_ != Output::OUTPUT_REPORT))
    return;
  this->frame_ = image;
  this->jpeg_ = jpeg;
  this->frame_sent_ = 0;
  this->frame_progress_ms_ = now;

  uint32_t elapsed_ms = this->last_frame_ms_ == 0 ? 0 : now - this->last_frame_ms_;
  this->rtp_timestamp_ += static_cast<uint32_t>((static_cast<uint64_t>(elapsed_ms) * RTP_CLOCK_RATE) / 1000);
  this->last_frame_ms_ = now;
#ifdef USE_RTSP_SERVER_AUDIO
  this->frame_time_ms_ = time_ms;
#endif
}

bool RTSPSession::queue_next_packet_(uint32_t now) {
#ifdef USE_RTSP_SERVER_AUDIO
  if (this->queue_sender_reports_(now))
    return true;
  // Audio and video packets alternate while both are waiting: audio is not delayed behind a whole frame, and a
  // client slower than the audio stream still gets the video, so it cannot hold the camera frame indefinitely.
  if (this->video_turn_ && this->queue_video_packet_()) {
    this->video_turn_ = false;
    return true;
  }
  if (this->queue_audio_packet_()) {
    this->video_turn_ = true;
    return true;
  }
#endif
  return this->queue_video_packet_();
}

bool RTSPSession::queue_video_packet_() {
  if (this->frame_ == nullptr)
    return false;
  if (this->frame_sent_ >= this->jpeg_.scan_len) {
    this->frame_.reset();  // its last packet is written
    return false;
  }

  const bool first_fragment = this->frame_sent_ == 0;
  const size_t header_len =
      INTERLEAVED_HEADER_LEN + RTP_HEADER_LEN + JPEG_HEADER_LEN + (first_fragment ? QUANT_HEADER_LEN : 0);
  const size_t qtables_len = first_fragment ? 2 * QTABLE_LEN : 0;
  const size_t payload_len =
      std::min(this->jpeg_.scan_len - this->frame_sent_, MAX_PACKET_SIZE - header_len - qtables_len);
  const bool last_fragment = this->frame_sent_ + payload_len >= this->jpeg_.scan_len;
  const size_t rtp_len = header_len - INTERLEAVED_HEADER_LEN + qtables_len + payload_len;

  auto *buf = reinterpret_cast<uint8_t *>(this->send_buf_.data());
  size_t pos = 0;
  buf[pos++] = '$';
  buf[pos++] = VIDEO_RTP_CHANNEL;
  buf[pos++] = static_cast<uint8_t>(rtp_len >> 8);
  buf[pos++] = static_cast<uint8_t>(rtp_len);

  pos = put_rtp_header(buf + pos, last_fragment, RTP_PAYLOAD_TYPE_JPEG, this->sequence_number_++, this->rtp_timestamp_,
                       this->ssrc_) -
        buf;

  // RFC 2435 main JPEG header
  buf[pos++] = 0;  // type-specific
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_ >> 16);
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_ >> 8);
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_);
  buf[pos++] = this->jpeg_.type;
  buf[pos++] = 255;  // Q >= 128: the quantization tables follow in the first packet
  buf[pos++] = static_cast<uint8_t>(this->jpeg_.width / 8);
  buf[pos++] = static_cast<uint8_t>(this->jpeg_.height / 8);

  uint8_t count = 0;
  if (first_fragment) {
    buf[pos++] = 0;  // MBZ
    buf[pos++] = 0;  // 8-bit precision
    buf[pos++] = static_cast<uint8_t>(qtables_len >> 8);
    buf[pos++] = static_cast<uint8_t>(qtables_len);
    this->iov_[0] = {buf, header_len};
    this->iov_[1] = {const_cast<uint8_t *>(this->jpeg_.qtable0), QTABLE_LEN};
    this->iov_[2] = {const_cast<uint8_t *>(this->jpeg_.qtable1), QTABLE_LEN};
    count = 3;
  } else {
    this->iov_[0] = {buf, header_len};
    count = 1;
  }
  this->iov_[count++] = {const_cast<uint8_t *>(this->jpeg_.scan_data + this->frame_sent_), payload_len};
  this->iov_pos_ = 0;
  this->iov_count_ = count;
  this->output_ = Output::OUTPUT_FRAME;
  this->frame_sent_ += payload_len;
#ifdef USE_RTSP_SERVER_AUDIO
  this->video_packets_++;
  this->video_octets_ += rtp_len - RTP_HEADER_LEN;
#endif
  return true;
}

#ifdef USE_RTSP_SERVER_AUDIO
bool RTSPSession::queue_audio_packet_() {
  const AudioTrack *track = this->audio_track_;
  if (!this->is_playing_audio() || this->audio_next_ == track->produced)
    return false;
  // The oldest packets are gone; a session that fell that far behind skips them.
  if (track->produced - this->audio_next_ > AUDIO_SLOTS)
    this->audio_next_ = track->produced - AUDIO_SLOTS;
  const uint32_t index = this->audio_next_++;
  // Timestamps count captured samples, so they stay exact however the packets are paced, and audio the device
  // lost shows as a gap.
  this->audio_timestamp_ = this->audio_rtp_offset_ + track->sample_pos[index % AUDIO_SLOTS];
  this->audio_capture_ms_ = track->capture_ms[index % AUDIO_SLOTS];

  auto *buf = reinterpret_cast<uint8_t *>(this->send_buf_.data());
  const size_t rtp_len = RTP_HEADER_LEN + track->payload_len;
  buf[0] = '$';
  buf[1] = AUDIO_RTP_CHANNEL;
  put_be16(buf + 2, rtp_len);
  put_rtp_header(buf + INTERLEAVED_HEADER_LEN, false, track->payload_type, this->audio_sequence_number_++,
                 this->audio_timestamp_, this->audio_ssrc_);
  this->iov_[0] = {buf, INTERLEAVED_HEADER_LEN + RTP_HEADER_LEN};
  this->iov_[1] = {const_cast<uint8_t *>(track->packet(index)), track->payload_len};
  this->iov_pos_ = 0;
  this->iov_count_ = 2;
  this->output_ = Output::OUTPUT_AUDIO;
  this->audio_in_flight_ = index;
  this->audio_packets_++;
  this->audio_octets_ += track->payload_len;
  return true;
}

bool RTSPSession::queue_sender_reports_(uint32_t now) {
  if (this->audio_packets_ == 0 || !this->is_playing_audio() ||
      (this->last_report_ms_ != 0 && now - this->last_report_ms_ < RTCP_INTERVAL_MS))
    return false;
  // With video set up, wait for its first packet too, so the first reports already pair the two tracks.
  const bool video = this->is_playing_video();
  if (video && this->video_packets_ == 0)
    return false;
  this->last_report_ms_ = now;

  auto *buf = reinterpret_cast<uint8_t *>(this->send_buf_.data());
  uint8_t *end = buf;
  if (video) {
    end = put_sender_report(end, VIDEO_RTCP_CHANNEL, this->ssrc_, this->frame_time_ms_, this->rtp_timestamp_,
                            this->video_packets_, this->video_octets_, this->session_id_);
  }
  end = put_sender_report(end, AUDIO_RTCP_CHANNEL, this->audio_ssrc_, this->audio_capture_ms_, this->audio_timestamp_,
                          this->audio_packets_, this->audio_octets_, this->session_id_);
  this->iov_[0] = {buf, static_cast<size_t>(end - buf)};
  this->iov_pos_ = 0;
  this->iov_count_ = 1;
  this->output_ = Output::OUTPUT_REPORT;
  return true;
}

void RTSPSession::on_audio_slot_reuse(uint32_t index) {
  if (this->output_ == Output::OUTPUT_AUDIO && this->audio_in_flight_ == index)
    this->detach_payload_();
}
#endif

void RTSPSession::drop_frame_() {
  this->frame_.reset();
  if (this->output_ == Output::OUTPUT_FRAME)
    this->detach_payload_();
}

void RTSPSession::detach_payload_() {
  this->filler_ = ZERO_FILL;
#ifdef USE_RTSP_SERVER_AUDIO
  if (this->output_ == Output::OUTPUT_AUDIO)
    this->filler_ = this->audio_track_->filler;
#endif
  this->output_ = Output::OUTPUT_CONTROL;
  if (this->iov_pos_ >= this->iov_count_)
    return;
  // Even an unwritten packet is kept, so output stays pending and on_new_frame() takes nothing until the
  // socket drains. Entries after the header point into the frame or an audio slot.
  for (uint8_t i = std::max<uint8_t>(this->iov_pos_, 1); i < this->iov_count_; i++)
    this->filler_len_ += this->iov_[i].iov_len;
  this->iov_count_ = std::max<uint8_t>(this->iov_pos_, 1);
}

bool RTSPSession::flush_(uint32_t now) {
  if (this->should_close_)
    return false;
  while (true) {
    if (this->iov_pos_ >= this->iov_count_) {
      if (this->filler_len_ == 0)
        return true;
      this->iov_pos_ = 0;
      this->iov_count_ = 0;
      while (this->filler_len_ > 0 && this->iov_count_ < this->iov_.size()) {
        const size_t len = std::min<size_t>(this->filler_len_, FILL_LEN);
        this->iov_[this->iov_count_++] = {const_cast<uint8_t *>(this->filler_), len};
        this->filler_len_ -= len;
      }
    }
    ssize_t written = this->socket_->writev(&this->iov_[this->iov_pos_], this->iov_count_ - this->iov_pos_);
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return false;  // retry next loop
    if (written <= 0) {
      if (written < 0) {
        ESP_LOGW(TAG, "Write failed: errno %d", errno);
      }
      this->should_close_ = true;
      return false;
    }
    this->last_activity_ms_ = now;
    // Only the frame's own packets count: a client that takes audio but no video still stalls the frame.
    if (this->output_ == Output::OUTPUT_FRAME)
      this->frame_progress_ms_ = now;
    // Step over what was written; a partial write leaves the rest of the current entry pending.
    auto remaining = static_cast<size_t>(written);
    while (remaining > 0) {
      struct iovec &entry = this->iov_[this->iov_pos_];
      if (remaining < entry.iov_len) {
        entry.iov_base = static_cast<uint8_t *>(entry.iov_base) + remaining;
        entry.iov_len -= remaining;
        break;
      }
      remaining -= entry.iov_len;
      this->iov_pos_++;
    }
  }
}

}  // namespace esphome::rtsp_server

#endif
