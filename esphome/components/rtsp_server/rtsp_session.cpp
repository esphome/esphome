#ifdef USE_ESP32

#include "rtsp_session.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <utility>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "rtsp_server.h"

namespace esphome::rtsp_server {

static const char *const TAG = "rtsp_server";

static constexpr uint8_t RTP_PAYLOAD_TYPE_JPEG = 26;
static constexpr uint32_t RTP_CLOCK_RATE = 90000;
static constexpr uint32_t SESSION_TIMEOUT_MS = 60000;
// RFC 2435 encodes width/height as 8-bit fields in units of 8 pixels, so
// dimensions above 255 * 8 = 2040 px cannot be represented in the payload header.
static constexpr uint16_t RTP_JPEG_MAX_DIMENSION = 2040;

namespace {

bool iequals(const StringRef &a, const StringRef &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

uint32_t parse_uint32(const StringRef &s) {
  uint32_t value = 0;
  for (char c : s) {
    if (c < '0' || c > '9')
      break;
    value = value * 10 + static_cast<uint32_t>(c - '0');
  }
  return value;
}

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

/// Result of scanning a JPEG frame's own headers -- the fields an RFC 2435 RTP/JPEG
/// payload header needs, plus where the entropy-coded scan data (the actual payload) starts.
struct JpegInfo {
  uint16_t width{0};
  uint16_t height{0};
  uint8_t type{0};  // RFC 2435 Type: 0 = 4:2:2 subsampling, 1 = 4:2:0
  const uint8_t *qtable0{nullptr};
  size_t qtable0_len{0};
  const uint8_t *qtable1{nullptr};
  size_t qtable1_len{0};
  size_t scan_data_offset{0};
};

/// Scans JPEG marker segments (SOF0/1, DQT, SOS) to locate dimensions, quantization
/// tables, and the start of the entropy-coded scan data. Assumes the encoder emits the
/// standard Huffman tables (true for the esp32-camera JPEG encoder/converter), so DHT
/// segments are not parsed -- receivers reconstruct them per RFC 2435. Does not handle
/// restart intervals (DRI); ESP32 camera JPEG output does not use them by default.
bool parse_jpeg_info(const uint8_t *data, size_t len, JpegInfo &out) {
  if (len < 4 || data[0] != 0xFF || data[1] != 0xD8)
    return false;  // missing SOI -- not a JPEG frame

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

    uint16_t seg_len = static_cast<uint16_t>((data[pos + 2] << 8) | data[pos + 3]);
    size_t seg_start = pos + 4;
    if (seg_len < 2 || seg_start + (seg_len - 2) > len)
      return false;
    size_t seg_end = seg_start + (seg_len - 2);

    if (marker == 0xC0 || marker == 0xC1) {  // SOF0 / SOF1 (baseline / extended sequential)
      if (seg_len < 8)
        return false;
      out.height = static_cast<uint16_t>((data[seg_start + 1] << 8) | data[seg_start + 2]);
      out.width = static_cast<uint16_t>((data[seg_start + 3] << 8) | data[seg_start + 4]);
      uint8_t num_components = data[seg_start + 5];
      if (num_components >= 1 && seg_start + 7 < seg_end) {
        uint8_t sampling = data[seg_start + 7];  // component 1 (Y) H/V sampling factors nibble
        out.type = (sampling == 0x22) ? 1 : 0;   // 2x2 -> 4:2:0, otherwise assume 4:2:2
      }
    } else if (marker == 0xDB) {  // DQT -- one or more quantization tables back to back
      size_t p = seg_start;
      while (p < seg_end) {
        uint8_t precision = data[p] >> 4;
        uint8_t table_id = data[p] & 0x0F;
        size_t table_len = precision == 0 ? 64 : 128;
        if (p + 1 + table_len > seg_end)
          break;
        if (precision == 0 && table_id == 0) {
          out.qtable0 = &data[p + 1];
          out.qtable0_len = table_len;
        } else if (precision == 0 && table_id == 1) {
          out.qtable1 = &data[p + 1];
          out.qtable1_len = table_len;
        }
        p += 1 + table_len;
      }
    } else if (marker == 0xDA) {  // SOS -- entropy-coded scan data follows immediately
      out.scan_data_offset = seg_end;
      return out.width > 0 && out.height > 0 && out.qtable0 != nullptr;
    }

    pos = seg_end;
  }
  return false;  // ran off the end of the buffer without finding SOS
}

}  // namespace

RTSPSession::RTSPSession(std::unique_ptr<socket::Socket> socket, RTSPServer *server, uint16_t server_port)
    : socket_(std::move(socket)), server_(server), server_port_(server_port) {
  this->socket_->setblocking(false);
  this->image_reader_.reset(camera::Camera::instance()->create_image_reader());
  snprintf(this->session_id_, sizeof(this->session_id_), "%08" PRIX32, random_uint32());
  this->ssrc_ = random_uint32();
  this->last_activity_ms_ = millis();
}

void RTSPSession::loop() {
  if (this->should_close_)
    return;

  if (this->send_offset_ < this->send_len_) {
    this->flush_send_buffer_();
    if (this->should_close_ || this->send_offset_ < this->send_len_)
      return;  // still backlogged (or just failed) -- retry next tick
    if (this->close_after_send_) {
      this->should_close_ = true;
      return;
    }
  }

  this->read_requests_();
  if (this->should_close_ || this->send_offset_ < this->send_len_)
    return;  // a response was just queued -- send it before producing more data

  // Drain the current frame until done or the socket backs up -- pacing one packet
  // per loop() tick would cap throughput at ~4 fps for a multi-packet VGA frame.
  while (this->state_ == State::PLAYING && this->frame_active_) {
    this->send_len_ = this->build_next_jpeg_packet_();
    this->send_offset_ = 0;
    this->flush_send_buffer_();
    if (this->should_close_ || this->send_offset_ < this->send_len_)
      break;  // backlogged -- the retry path at the top of loop() resumes the flush
  }

  if (millis() - this->last_activity_ms_ > SESSION_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Session %s timed out", this->session_id_);
    this->should_close_ = true;
  }
}

void RTSPSession::read_requests_() {
  if (!this->socket_->ready())
    return;

  // The last buffer byte is reserved for a NUL terminator, which StringRef::find()
  // (strstr-based) requires on the underlying storage.
  const size_t buf_capacity = this->request_buf_.size() - 1;
  while (true) {
    if (this->request_len_ >= buf_capacity) {
      ESP_LOGW(TAG, "Request too large, closing session");
      this->should_close_ = true;
      return;
    }
    ssize_t n = this->socket_->read(this->request_buf_.data() + this->request_len_, buf_capacity - this->request_len_);
    if (n > 0) {
      this->request_len_ += static_cast<size_t>(n);
      this->last_activity_ms_ = millis();
      continue;
    }
    if (n == 0) {
      this->should_close_ = true;
      return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }
    ESP_LOGW(TAG, "Read failed: errno %d", errno);
    this->should_close_ = true;
    return;
  }

  this->request_buf_[this->request_len_] = '\0';
  StringRef buf(this->request_buf_.data(), this->request_len_);
  size_t terminator = buf.find("\r\n\r\n");
  if (terminator == std::string::npos)
    return;  // wait for the rest of the request

  this->handle_request_(StringRef(buf.c_str(), terminator));
  // v1 limitation: any bytes pipelined after this request are discarded. RTSP clients
  // send one request and wait for the response, so this does not affect real clients.
  this->request_len_ = 0;
}

void RTSPSession::handle_request_(StringRef request) {
  // Sub-slices are built with the (pointer, length) constructor rather than substr(),
  // which would heap-allocate a std::string copy.
  size_t line_end = request.find("\r\n");
  StringRef first_line = line_end == std::string::npos ? request : StringRef(request.c_str(), line_end);

  size_t sp1 = first_line.find(' ');
  size_t sp2 = sp1 == std::string::npos ? std::string::npos : first_line.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) {
    this->cseq_ = 0;
    this->send_simple_response_(400, "Bad Request");
    return;
  }
  StringRef method(first_line.c_str(), sp1);

  this->cseq_ = 0;
  StringRef transport;

  size_t pos = line_end == std::string::npos ? request.size() : line_end + 2;
  while (pos < request.size()) {
    size_t next = request.find("\r\n", pos);
    size_t header_end = next == std::string::npos ? request.size() : next;
    StringRef header(request.c_str() + pos, header_end - pos);

    size_t colon = header.find(':');
    if (colon != std::string::npos) {
      StringRef key = trim(StringRef(header.c_str(), colon));
      StringRef value = trim(StringRef(header.c_str() + colon + 1, header.size() - colon - 1));
      if (iequals(key, StringRef::from_lit("CSeq"))) {
        this->cseq_ = parse_uint32(value);
      } else if (iequals(key, StringRef::from_lit("Transport"))) {
        transport = value;
      }
    }

    if (next == std::string::npos)
      break;
    pos = next + 2;
  }

  if (iequals(method, StringRef::from_lit("OPTIONS"))) {
    this->handle_options_();
  } else if (iequals(method, StringRef::from_lit("DESCRIBE"))) {
    this->handle_describe_();
  } else if (iequals(method, StringRef::from_lit("SETUP"))) {
    this->handle_setup_(transport);
  } else if (iequals(method, StringRef::from_lit("PLAY"))) {
    this->handle_play_();
  } else if (iequals(method, StringRef::from_lit("TEARDOWN"))) {
    this->handle_teardown_();
  } else if (iequals(method, StringRef::from_lit("GET_PARAMETER"))) {
    this->handle_get_parameter_();
  } else {
    this->send_simple_response_(501, "Not Implemented");
  }
}

void RTSPSession::send_simple_response_(uint16_t code, const char *reason) {
  int len = snprintf(reinterpret_cast<char *>(this->send_buf_.data()), this->send_buf_.size(),
                     "RTSP/1.0 %u %s\r\nCSeq: %" PRIu32 "\r\n\r\n", code, reason, this->cseq_);
  this->send_len_ = len > 0 ? std::min<size_t>(static_cast<size_t>(len), this->send_buf_.size()) : 0;
  this->send_offset_ = 0;
}

void RTSPSession::handle_options_() {
  int len = snprintf(reinterpret_cast<char *>(this->send_buf_.data()), this->send_buf_.size(),
                     "RTSP/1.0 200 OK\r\nCSeq: %" PRIu32 "\r\n"
                     "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER\r\n\r\n",
                     this->cseq_);
  this->send_len_ = len > 0 ? std::min<size_t>(static_cast<size_t>(len), this->send_buf_.size()) : 0;
  this->send_offset_ = 0;
}

void RTSPSession::handle_describe_() {
  char addr_buf[socket::SOCKADDR_STR_LEN];
  this->socket_->getsockname_to(addr_buf);

  char body[192];
  int body_len = snprintf(body, sizeof(body),
                          "v=0\r\n"
                          "o=- 0 0 IN IP4 0.0.0.0\r\n"
                          "s=ESPHome RTSP\r\n"
                          "t=0 0\r\n"
                          "m=video 0 RTP/AVP 26\r\n"
                          "a=control:trackID=0\r\n");
  if (body_len < 0) {
    body_len = 0;
    body[0] = '\0';  // contents are undefined on encoding error -- make %s safe
  } else if (static_cast<size_t>(body_len) >= sizeof(body)) {
    // snprintf reports the untruncated length; Content-Length must match the bytes
    // actually appended below or clients hang waiting for the difference.
    body_len = sizeof(body) - 1;
  }

  int len = snprintf(reinterpret_cast<char *>(this->send_buf_.data()), this->send_buf_.size(),
                     "RTSP/1.0 200 OK\r\nCSeq: %" PRIu32 "\r\n"
                     "Content-Base: rtsp://%s:%u/\r\n"
                     "Content-Type: application/sdp\r\n"
                     "Content-Length: %d\r\n\r\n%s",
                     this->cseq_, addr_buf, this->server_port_, body_len, body);
  this->send_len_ = len > 0 ? std::min<size_t>(static_cast<size_t>(len), this->send_buf_.size()) : 0;
  this->send_offset_ = 0;
}

void RTSPSession::handle_setup_(StringRef transport_header) {
  if (transport_header.find("RTP/AVP/TCP") == std::string::npos) {
    // Only RTP interleaved over the RTSP TCP connection is supported (no UDP transport).
    this->send_simple_response_(461, "Unsupported Transport");
    return;
  }

  // A SETUP while streaming implicitly stops playback: release the play accounting
  // and any in-flight frame, ending in READY (also the correct state after a
  // first-time SETUP from INIT).
  this->stop_playing_();

  // Every session carries exactly one video track, so the interleaved channel numbers
  // are always 0 (RTP) / 1 (RTCP, unused) regardless of what the client proposed.
  int len = snprintf(reinterpret_cast<char *>(this->send_buf_.data()), this->send_buf_.size(),
                     "RTSP/1.0 200 OK\r\nCSeq: %" PRIu32 "\r\n"
                     "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                     "Session: %s;timeout=60\r\n\r\n",
                     this->cseq_, this->session_id_);
  this->send_len_ = len > 0 ? std::min<size_t>(static_cast<size_t>(len), this->send_buf_.size()) : 0;
  this->send_offset_ = 0;
}

void RTSPSession::handle_play_() {
  if (this->state_ != State::READY && this->state_ != State::PLAYING) {
    this->send_simple_response_(455, "Method Not Valid In This State");
    return;
  }

  this->start_playing_();

  int len = snprintf(reinterpret_cast<char *>(this->send_buf_.data()), this->send_buf_.size(),
                     "RTSP/1.0 200 OK\r\nCSeq: %" PRIu32 "\r\nSession: %s;timeout=60\r\nRange: npt=0.000-\r\n\r\n",
                     this->cseq_, this->session_id_);
  this->send_len_ = len > 0 ? std::min<size_t>(static_cast<size_t>(len), this->send_buf_.size()) : 0;
  this->send_offset_ = 0;
}

void RTSPSession::handle_teardown_() {
  this->stop_playing_();
  this->send_simple_response_(200, "OK");
  this->close_after_send_ = true;
}

void RTSPSession::handle_get_parameter_() { this->send_simple_response_(200, "OK"); }

void RTSPSession::start_playing_() {
  this->state_ = State::PLAYING;
  if (!this->playing_counted_) {
    this->playing_counted_ = true;
    this->server_->session_started_playing();
  }
  this->sequence_number_ = static_cast<uint16_t>(random_uint32());
  this->rtp_timestamp_ = random_uint32();
  // A repeated PLAY may arrive mid-frame; the framebuffer must go back to the
  // camera pool or capture stalls device-wide.
  this->drop_active_frame_();
}

void RTSPSession::stop_playing_() {
  this->state_ = State::READY;
  if (this->playing_counted_) {
    this->playing_counted_ = false;
    this->server_->session_stopped_playing();
  }
  this->drop_active_frame_();
}

void RTSPSession::drop_active_frame_() {
  if (this->frame_active_ && this->image_reader_) {
    this->image_reader_->return_image();
    this->frame_active_ = false;
  }
}

void RTSPSession::ensure_stream_stopped() { this->stop_playing_(); }

void RTSPSession::on_new_frame(const std::shared_ptr<camera::CameraImage> &image) {
  if (this->state_ != State::PLAYING)
    return;

  // Freshest-frame-wins: replacing the reader's image (rather than queuing) drops
  // whatever was still in flight so a slow session can't stall camera capture for
  // everyone else -- esp32_camera only re-arms once the shared_ptr refcount drops to 1.
  this->image_reader_->set_image(image);
  size_t total_len = this->image_reader_->available();
  const uint8_t *data = this->image_reader_->peek_data_buffer();

  JpegInfo info;
  if (!parse_jpeg_info(data, total_len, info)) {
    ESP_LOGW(TAG, "Failed to parse JPEG frame, dropping");
    this->image_reader_->return_image();
    this->frame_active_ = false;
    return;
  }

  if (info.width > RTP_JPEG_MAX_DIMENSION || info.height > RTP_JPEG_MAX_DIMENSION) {
    // Warn once per session -- this repeats for every frame until the camera
    // resolution is lowered, and logging at frame rate would flood the log.
    if (!this->oversize_frame_warned_) {
      this->oversize_frame_warned_ = true;
      ESP_LOGW(TAG,
               "Frame %" PRIu16 "x%" PRIu16 " exceeds the RTP/JPEG limit of %" PRIu16
               " px per side, dropping; lower the camera resolution",
               info.width, info.height, RTP_JPEG_MAX_DIMENSION);
    }
    this->image_reader_->return_image();
    this->frame_active_ = false;
    return;
  }

  size_t scan_len = total_len - info.scan_data_offset;
  const uint8_t *scan_data = data + info.scan_data_offset;
  if (scan_len >= 2 && scan_data[scan_len - 2] == 0xFF && scan_data[scan_len - 1] == 0xD9) {
    scan_len -= 2;  // strip trailing EOI -- RFC 2435 payload is entropy data only
  }

  this->image_reader_->consume_data(info.scan_data_offset);

  this->jpeg_width_ = info.width;
  this->jpeg_height_ = info.height;
  this->jpeg_type_ = info.type;
  this->qtable0_ = info.qtable0;
  this->qtable0_len_ = info.qtable0_len;
  // Grayscale JPEGs may only carry one quantization table -- reuse it for chroma.
  this->qtable1_ = info.qtable1 != nullptr ? info.qtable1 : info.qtable0;
  this->qtable1_len_ = info.qtable1 != nullptr ? info.qtable1_len : info.qtable0_len;
  this->frame_scan_len_ = scan_len;
  this->frame_sent_ = 0;

  uint32_t now = millis();
  uint32_t elapsed_ms = this->last_frame_ms_ == 0 ? 0 : now - this->last_frame_ms_;
  this->rtp_timestamp_ += static_cast<uint32_t>((static_cast<uint64_t>(elapsed_ms) * RTP_CLOCK_RATE) / 1000);
  this->last_frame_ms_ = now;

  this->frame_active_ = true;
}

size_t RTSPSession::build_next_jpeg_packet_() {
  bool first_fragment = this->frame_sent_ == 0;
  size_t quant_header_len = first_fragment ? 4 + this->qtable0_len_ + this->qtable1_len_ : 0;

  size_t fixed_overhead = 4 /* interleave */ + 12 /* RTP */ + 8 /* JPEG main header */ + quant_header_len;
  size_t max_payload = this->send_buf_.size() - fixed_overhead;
  size_t remaining = this->frame_scan_len_ - this->frame_sent_;
  size_t payload_len = std::min(remaining, max_payload);
  bool last_fragment = (this->frame_sent_ + payload_len) >= this->frame_scan_len_;

  uint8_t *buf = this->send_buf_.data();
  size_t pos = 0;

  buf[pos++] = '$';
  buf[pos++] = 0;  // interleaved channel 0 -- this session's single RTP video track
  size_t len_field_pos = pos;
  pos += 2;  // patched with the RTP packet length once known

  size_t rtp_start = pos;
  buf[pos++] = 0x80;  // V=2, P=0, X=0, CC=0
  buf[pos++] = static_cast<uint8_t>((last_fragment ? 0x80 : 0x00) | RTP_PAYLOAD_TYPE_JPEG);
  buf[pos++] = static_cast<uint8_t>(this->sequence_number_ >> 8);
  buf[pos++] = static_cast<uint8_t>(this->sequence_number_);
  this->sequence_number_++;
  buf[pos++] = static_cast<uint8_t>(this->rtp_timestamp_ >> 24);
  buf[pos++] = static_cast<uint8_t>(this->rtp_timestamp_ >> 16);
  buf[pos++] = static_cast<uint8_t>(this->rtp_timestamp_ >> 8);
  buf[pos++] = static_cast<uint8_t>(this->rtp_timestamp_);
  buf[pos++] = static_cast<uint8_t>(this->ssrc_ >> 24);
  buf[pos++] = static_cast<uint8_t>(this->ssrc_ >> 16);
  buf[pos++] = static_cast<uint8_t>(this->ssrc_ >> 8);
  buf[pos++] = static_cast<uint8_t>(this->ssrc_);

  // RFC 2435 main JPEG header
  buf[pos++] = 0;  // type-specific
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_ >> 16);
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_ >> 8);
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_);
  buf[pos++] = this->jpeg_type_;
  buf[pos++] = 255;  // Q >= 128 signals an explicit quantization table header below
  buf[pos++] = static_cast<uint8_t>(this->jpeg_width_ / 8);
  buf[pos++] = static_cast<uint8_t>(this->jpeg_height_ / 8);

  if (first_fragment) {
    uint16_t total_qlen = static_cast<uint16_t>(this->qtable0_len_ + this->qtable1_len_);
    buf[pos++] = 0;  // MBZ
    buf[pos++] = 0;  // precision: 8-bit
    buf[pos++] = static_cast<uint8_t>(total_qlen >> 8);
    buf[pos++] = static_cast<uint8_t>(total_qlen);
    memcpy(buf + pos, this->qtable0_, this->qtable0_len_);
    pos += this->qtable0_len_;
    memcpy(buf + pos, this->qtable1_, this->qtable1_len_);
    pos += this->qtable1_len_;
  }

  memcpy(buf + pos, this->image_reader_->peek_data_buffer(), payload_len);
  this->image_reader_->consume_data(payload_len);
  pos += payload_len;

  size_t rtp_len = pos - rtp_start;
  buf[len_field_pos] = static_cast<uint8_t>(rtp_len >> 8);
  buf[len_field_pos + 1] = static_cast<uint8_t>(rtp_len);

  this->frame_sent_ += payload_len;
  if (last_fragment) {
    this->image_reader_->return_image();
    this->frame_active_ = false;
  }

  return pos;
}

void RTSPSession::flush_send_buffer_() {
  while (this->send_offset_ < this->send_len_) {
    ssize_t written =
        this->socket_->write(this->send_buf_.data() + this->send_offset_, this->send_len_ - this->send_offset_);
    if (written > 0) {
      this->send_offset_ += static_cast<size_t>(written);
      this->last_activity_ms_ = millis();
      continue;
    }
    if (written == 0) {
      this->should_close_ = true;
      return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;  // retry next loop() tick
    }
    ESP_LOGW(TAG, "Write failed: errno %d", errno);
    this->should_close_ = true;
    return;
  }
  this->send_len_ = 0;
  this->send_offset_ = 0;
}

}  // namespace esphome::rtsp_server

#endif
