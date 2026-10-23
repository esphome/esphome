#include "rtsp_session.h"

#ifdef USE_CAMERA

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

#ifdef USE_RTSP_H264
#include <esp_tls_crypto.h>
#include "esphome/components/camera_video/h264_annexb.h"
#endif
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

static constexpr uint8_t RTP_PAYLOAD_TYPE_JPEG = 26;
#ifdef USE_RTSP_H264
static constexpr uint8_t RTP_PAYLOAD_TYPE_H264 = 96;
static constexpr uint8_t H264_FU_A_NAL_TYPE = 28;
static constexpr uint8_t H264_FU_START_BIT = 0x80;
static constexpr uint8_t H264_FU_END_BIT = 0x40;
static constexpr size_t H264_FU_HEADER_LEN = 2;
static constexpr uint32_t MICROSECONDS_PER_SECOND = 1000000;
static constexpr size_t BASE64_PARAMETER_SET_BYTES = 4U * ((camera_video::H264_PARAMETER_SET_MAX_SIZE + 2U) / 3U) + 1U;
#endif
static constexpr uint32_t RTP_CLOCK_RATE = 90000;
static constexpr uint32_t SESSION_TIMEOUT_MS = 60000;
static constexpr char REQUEST_END[] = "\r\n\r\n";
static constexpr size_t REQUEST_END_LEN = sizeof(REQUEST_END) - 1;
static constexpr uint8_t ZERO_FILL[64]{};
static constexpr char SDP_BODY[] = "v=0\r\n"
                                   "o=- 0 0 IN IP4 0.0.0.0\r\n"
                                   "s=ESPHome RTSP\r\n"
                                   "t=0 0\r\n"
                                   "m=video 0 RTP/AVP 26\r\n"
                                   "a=control:trackID=0\r\n";

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

#ifdef USE_RTSP_H264
size_t encode_base64_to(char *output, size_t output_size, const uint8_t *input, size_t input_size) {
  if (output == nullptr || output_size == 0 || input == nullptr || input_size == 0)
    return 0;
  size_t encoded_length = 0;
  if (esp_crypto_base64_encode(reinterpret_cast<uint8_t *>(output), output_size, &encoded_length, input, input_size) !=
          0 ||
      encoded_length >= output_size) {
    output[0] = '\0';
    return 0;
  }
  output[encoded_length] = '\0';
  return encoded_length;
}

size_t write_h264_rtp_header(uint8_t *buf, size_t payload_len, bool marker, uint16_t sequence_number,
                             uint32_t timestamp, uint32_t ssrc) {
  const size_t rtp_len = RTP_HEADER_LEN + payload_len;
  size_t pos = 0;
  buf[pos++] = '$';
  buf[pos++] = 0;
  buf[pos++] = static_cast<uint8_t>(rtp_len >> 8);
  buf[pos++] = static_cast<uint8_t>(rtp_len);
  buf[pos++] = 0x80;
  buf[pos++] = static_cast<uint8_t>((marker ? 0x80 : 0x00) | RTP_PAYLOAD_TYPE_H264);
  buf[pos++] = static_cast<uint8_t>(sequence_number >> 8);
  buf[pos++] = static_cast<uint8_t>(sequence_number);
  buf[pos++] = static_cast<uint8_t>(timestamp >> 24);
  buf[pos++] = static_cast<uint8_t>(timestamp >> 16);
  buf[pos++] = static_cast<uint8_t>(timestamp >> 8);
  buf[pos++] = static_cast<uint8_t>(timestamp);
  buf[pos++] = static_cast<uint8_t>(ssrc >> 24);
  buf[pos++] = static_cast<uint8_t>(ssrc >> 16);
  buf[pos++] = static_cast<uint8_t>(ssrc >> 8);
  buf[pos++] = static_cast<uint8_t>(ssrc);
  return pos;
}
#endif

}  // namespace

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

#ifdef USE_RTSP_H264
void RTSPSession::open(std::unique_ptr<socket::Socket> socket, uint32_t now, camera_video::H264Stream *h264_stream,
                       uint16_t h264_payload_size) {
  this->h264_stream_ = h264_stream;
  this->h264_payload_size_ = h264_payload_size;
#else
void RTSPSession::open(std::unique_ptr<socket::Socket> socket, uint32_t now) {
#endif
  this->socket_ = std::move(socket);
  this->socket_->setblocking(false);
  int enable = 1;
  this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
  this->state_ = State::INIT;
  this->should_close_ = false;
  this->close_after_send_ = false;
  this->read_pending_ = false;
  this->request_len_ = 0;
  this->skip_len_ = 0;
  this->filler_len_ = 0;
  this->iov_pos_ = 0;
  this->iov_count_ = 0;
  this->frame_.reset();
  this->cseq_ = 0;
  this->last_frame_ms_ = 0;
  this->last_activity_ms_ = now;
  this->ssrc_ = random_uint32();
  this->sequence_number_ = static_cast<uint16_t>(random_uint32());
  this->rtp_timestamp_ = random_uint32();
#ifdef USE_RTSP_H264
  this->clear_h264_frame_();
  this->h264_timestamp_origin_us_ = 0;
  this->h264_timestamp_origin_rtp_ = this->rtp_timestamp_;
  this->waiting_for_keyframe_ = this->h264_stream_ != nullptr;
#endif
  format_hex_to(this->session_id_, random_uint32());
}

void RTSPSession::close_() {
  this->frame_.reset();
#ifdef USE_RTSP_H264
  this->clear_h264_frame_();
#endif
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
  if (this->has_frame_() && now - this->frame_progress_ms_ > FRAME_STALL_MS) {
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
    if (!this->queue_next_packet_())
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
    size_t sdp_length = sizeof(SDP_BODY) - 1;
#ifdef USE_RTSP_H264
    if (this->h264_stream_ != nullptr) {
      if (!this->prepare_h264_sdp_()) {
        this->queue_response_(503, "Service Unavailable");
        return;
      }
      sdp = this->sdp_buf_.data();
      sdp_length = std::strlen(sdp);
    }
#endif
    this->queue_response_(200, "OK",
                          "Content-Base: rtsp://%s%s%s:%u/\r\n"
                          "Content-Type: application/sdp\r\n"
                          "Content-Length: %u\r\n",
                          ipv6 ? "[" : "", addr_buf, ipv6 ? "]" : "", port, static_cast<unsigned>(sdp_length));
    this->iov_[this->iov_count_++] = {const_cast<char *>(sdp), sdp_length};
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("SETUP"))) {
    this->handle_setup_(transport);
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
  this->sending_packet_ = false;
}

void RTSPSession::handle_setup_(StringRef transport_header) {
  if (!contains(transport_header, StringRef::from_lit("RTP/AVP/TCP"))) {
    this->queue_response_(461, "Unsupported Transport");
    return;
  }
  // A SETUP while streaming stops playback, ending in READY like a first SETUP.
  this->stop_playing_();
  // The single video track always uses interleaved channels 0 (RTP) and 1 (RTCP, unused).
  this->queue_response_(200, "OK", "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n");
}

void RTSPSession::handle_play_() {
  if (this->state_ == State::INIT) {
    this->queue_response_(455, "Method Not Valid In This State");
    return;
  }
  this->state_ = State::PLAYING;
  this->frame_.reset();  // a repeated PLAY may arrive mid-frame
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr) {
    this->clear_h264_frame_();
    this->waiting_for_keyframe_ = true;
    this->h264_stream_->request_keyframe();
  }
#endif
  this->queue_response_(200, "OK", "Range: npt=0.000-\r\n");
}

void RTSPSession::stop_playing_() {
  this->state_ = State::READY;
  this->frame_.reset();
#ifdef USE_RTSP_H264
  this->clear_h264_frame_();
  this->waiting_for_keyframe_ = this->h264_stream_ != nullptr;
#endif
}

void RTSPSession::on_new_frame(const std::shared_ptr<camera::CameraImage> &image, const JpegInfo &jpeg, uint32_t now) {
  // Pending output means a frame is in flight or a stalled one is still being padded out.
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr)
    return;
#endif
  if (this->state_ != State::PLAYING || this->has_frame_() || this->iov_pos_ < this->iov_count_ ||
      this->filler_len_ > 0)
    return;
  this->frame_ = image;
  this->jpeg_ = jpeg;
  this->frame_sent_ = 0;
  this->frame_progress_ms_ = now;
  uint32_t elapsed_ms = this->last_frame_ms_ == 0 ? 0 : now - this->last_frame_ms_;
  this->rtp_timestamp_ += static_cast<uint32_t>((static_cast<uint64_t>(elapsed_ms) * RTP_CLOCK_RATE) / 1000);
  this->last_frame_ms_ = now;
}

#ifdef USE_RTSP_H264
void RTSPSession::on_new_h264_frame(const camera_video::FrameRef<camera_video::H264Frame> &frame,
                                    const camera_video::H264CodecConfig *codec_config, uint32_t now) {
  if (this->h264_stream_ == nullptr || frame == nullptr || this->state_ != State::PLAYING)
    return;
  if (this->has_frame_() || this->iov_pos_ < this->iov_count_ || this->filler_len_ > 0) {
    this->mark_h264_discontinuity_();
    return;
  }
  if (this->waiting_for_keyframe_ && !frame->is_keyframe())
    return;
  if (frame->is_keyframe())
    this->waiting_for_keyframe_ = false;

  this->h264_frame_ = frame;
  this->h264_cursor_ = frame->get_data_buffer();
  this->h264_end_ = this->h264_cursor_ + frame->get_data_length();
  this->h264_nal_data_ = nullptr;
  this->h264_nal_length_ = 0;
  this->h264_nal_offset_ = 0;
  if (frame->is_keyframe() && codec_config != nullptr && codec_config->is_valid()) {
    this->h264_codec_config_ = *codec_config;
    this->h264_stage_ = H264Stage::CONFIG_SPS;
  } else {
    this->h264_stage_ = H264Stage::FRAME;
  }

  uint64_t timestamp_us = frame->get_timestamp_us();
  if (timestamp_us == 0)
    timestamp_us = static_cast<uint64_t>(now) * 1000ULL;
  if (this->h264_timestamp_origin_us_ == 0 || timestamp_us < this->h264_timestamp_origin_us_) {
    this->h264_timestamp_origin_us_ = timestamp_us;
    this->h264_timestamp_origin_rtp_ = this->rtp_timestamp_;
  }
  const uint64_t elapsed_us = timestamp_us - this->h264_timestamp_origin_us_;
  this->rtp_timestamp_ =
      this->h264_timestamp_origin_rtp_ + static_cast<uint32_t>(elapsed_us * RTP_CLOCK_RATE / MICROSECONDS_PER_SECOND);
  this->last_frame_ms_ = now;
  this->frame_progress_ms_ = now;
}
void RTSPSession::on_h264_discontinuity() {
  if (this->h264_stream_ != nullptr && this->state_ == State::PLAYING)
    this->mark_h264_discontinuity_();
}
#endif

bool RTSPSession::has_frame_() const {
#ifdef USE_RTSP_H264
  if (this->h264_frame_ != nullptr)
    return true;
#endif
  return this->frame_ != nullptr;
}

bool RTSPSession::queue_next_packet_() {
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr)
    return this->queue_next_h264_packet_();
#endif
  return this->queue_next_jpeg_packet_();
}

bool RTSPSession::queue_next_jpeg_packet_() {
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
  buf[pos++] = 0;  // interleaved channel 0
  buf[pos++] = static_cast<uint8_t>(rtp_len >> 8);
  buf[pos++] = static_cast<uint8_t>(rtp_len);

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
  this->sending_packet_ = true;
  this->frame_sent_ += payload_len;
  return true;
}

#ifdef USE_RTSP_H264
bool RTSPSession::prepare_h264_sdp_() {
  camera_video::H264CodecConfig config;
  if (this->h264_stream_ == nullptr || !this->h264_stream_->get_h264_codec_config(&config))
    return false;
  const auto info = this->h264_stream_->get_stream_info();
  char sps[BASE64_PARAMETER_SET_BYTES];
  char pps[BASE64_PARAMETER_SET_BYTES];
  if (encode_base64_to(sps, sizeof(sps), config.sps.data(), config.sps_length) == 0 ||
      encode_base64_to(pps, sizeof(pps), config.pps.data(), config.pps_length) == 0)
    return false;
  const int length =
      snprintf(this->sdp_buf_.data(), this->sdp_buf_.size(),
               "v=0\r\n"
               "o=- 0 0 IN IP4 0.0.0.0\r\n"
               "s=ESPHome RTSP\r\n"
               "t=0 0\r\n"
               "m=video 0 RTP/AVP %u\r\n"
               "c=IN IP4 0.0.0.0\r\n"
               "a=rtpmap:%u H264/%" PRIu32 "\r\n"
               "a=framerate:%u\r\n"
               "a=fmtp:%u packetization-mode=1;profile-level-id=%06" PRIX32 ";sprop-parameter-sets=%s,%s\r\n"
               "a=control:trackID=0\r\n",
               RTP_PAYLOAD_TYPE_H264, RTP_PAYLOAD_TYPE_H264, RTP_CLOCK_RATE, static_cast<unsigned>(info.fps),
               RTP_PAYLOAD_TYPE_H264, config.profile_level_id & 0xFFFFFFU, sps, pps);
  return length > 0 && static_cast<size_t>(length) < this->sdp_buf_.size();
}

bool RTSPSession::load_next_h264_nal_() {
  while (true) {
    this->h264_nal_offset_ = 0;
    this->h264_nal_last_ = false;
    if (this->h264_stage_ == H264Stage::CONFIG_SPS) {
      this->h264_nal_data_ = this->h264_codec_config_.sps.data();
      this->h264_nal_length_ = this->h264_codec_config_.sps_length;
      this->h264_stage_ = H264Stage::CONFIG_PPS;
      return this->h264_nal_length_ != 0;
    }
    if (this->h264_stage_ == H264Stage::CONFIG_PPS) {
      this->h264_nal_data_ = this->h264_codec_config_.pps.data();
      this->h264_nal_length_ = this->h264_codec_config_.pps_length;
      this->h264_stage_ = H264Stage::FRAME;
      return this->h264_nal_length_ != 0;
    }
    if (this->h264_stage_ != H264Stage::FRAME || this->h264_frame_ == nullptr) {
      this->clear_h264_frame_();
      return false;
    }

    camera_video::H264AnnexBNal nal;
    if (!camera_video::next_annexb_nal(&this->h264_cursor_, this->h264_end_, &nal)) {
      this->clear_h264_frame_();
      return false;
    }
    this->h264_nal_data_ = nal.data;
    this->h264_nal_length_ = nal.len;
    const uint8_t *peek = this->h264_cursor_;
    camera_video::H264AnnexBNal next;
    this->h264_nal_last_ = !camera_video::next_annexb_nal(&peek, this->h264_end_, &next);
    if (this->h264_nal_length_ != 0)
      return true;
  }
}

bool RTSPSession::queue_next_h264_packet_() {
  if (this->h264_frame_ == nullptr)
    return false;
  if (this->h264_nal_data_ == nullptr && !this->load_next_h264_nal_())
    return false;

  auto *buf = reinterpret_cast<uint8_t *>(this->send_buf_.data());
  if (this->h264_nal_length_ <= this->h264_payload_size_) {
    const bool marker = this->h264_nal_last_;
    const size_t header_len = write_h264_rtp_header(buf, this->h264_nal_length_, marker, this->sequence_number_++,
                                                    this->rtp_timestamp_, this->ssrc_);
    this->iov_[0] = {buf, header_len};
    this->iov_[1] = {const_cast<uint8_t *>(this->h264_nal_data_), this->h264_nal_length_};
    this->iov_pos_ = 0;
    this->iov_count_ = 2;
    this->sending_packet_ = true;
    this->h264_nal_data_ = nullptr;
    this->h264_nal_length_ = 0;
    return true;
  }

  if (this->h264_nal_length_ <= 1 || this->h264_payload_size_ <= H264_FU_HEADER_LEN) {
    ESP_LOGW(TAG, "Invalid H.264 NAL/payload size (%zu/%zu)", this->h264_nal_length_, this->h264_payload_size_);
    this->mark_h264_discontinuity_();
    this->clear_h264_frame_();
    return false;
  }

  const size_t maximum_fragment = this->h264_payload_size_ - H264_FU_HEADER_LEN;
  const size_t payload_bytes = this->h264_nal_length_ - 1;
  const size_t remaining = payload_bytes - this->h264_nal_offset_;
  const size_t fragment_length = std::min(remaining, maximum_fragment);
  const bool first_fragment = this->h264_nal_offset_ == 0;
  const bool last_fragment = fragment_length == remaining;
  const bool marker = this->h264_nal_last_ && last_fragment;
  const uint8_t nal_header = this->h264_nal_data_[0];
  const size_t header_len = write_h264_rtp_header(buf, H264_FU_HEADER_LEN + fragment_length, marker,
                                                  this->sequence_number_++, this->rtp_timestamp_, this->ssrc_);
  buf[header_len] = static_cast<uint8_t>((nal_header & 0xE0U) | H264_FU_A_NAL_TYPE);
  buf[header_len + 1] =
      static_cast<uint8_t>((nal_header & camera_video::H264_NAL_TYPE_MASK) | (first_fragment ? H264_FU_START_BIT : 0U) |
                           (last_fragment ? H264_FU_END_BIT : 0U));
  this->iov_[0] = {buf, header_len + H264_FU_HEADER_LEN};
  this->iov_[1] = {const_cast<uint8_t *>(this->h264_nal_data_ + 1 + this->h264_nal_offset_), fragment_length};
  this->iov_pos_ = 0;
  this->iov_count_ = 2;
  this->sending_packet_ = true;
  this->h264_nal_offset_ += fragment_length;
  if (last_fragment) {
    this->h264_nal_data_ = nullptr;
    this->h264_nal_length_ = 0;
    this->h264_nal_offset_ = 0;
  }
  return true;
}

void RTSPSession::mark_h264_discontinuity_() {
  if (this->waiting_for_keyframe_)
    return;
  this->waiting_for_keyframe_ = true;
  if (this->h264_stream_ != nullptr)
    this->h264_stream_->request_keyframe();
}

void RTSPSession::clear_h264_frame_() {
  this->h264_frame_.reset();
  this->h264_cursor_ = nullptr;
  this->h264_end_ = nullptr;
  this->h264_nal_data_ = nullptr;
  this->h264_nal_length_ = 0;
  this->h264_nal_offset_ = 0;
  this->h264_nal_last_ = false;
  this->h264_stage_ = H264Stage::DONE;
}
#endif

void RTSPSession::drop_frame_() {
  this->frame_.reset();
#ifdef USE_RTSP_H264
  if (this->h264_frame_ != nullptr)
    this->mark_h264_discontinuity_();
  this->clear_h264_frame_();
#endif
  if (!this->sending_packet_ || this->iov_pos_ >= this->iov_count_)
    return;
  // Even an unwritten packet is kept, so output stays pending and on_new_frame() takes nothing until the
  // socket drains. Entries after the header point into the frame.
  for (uint8_t i = std::max<uint8_t>(this->iov_pos_, 1); i < this->iov_count_; i++)
    this->filler_len_ += this->iov_[i].iov_len;
  this->iov_count_ = std::max<uint8_t>(this->iov_pos_, 1);
  this->sending_packet_ = false;
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
        const size_t len = std::min<size_t>(this->filler_len_, sizeof(ZERO_FILL));
        this->iov_[this->iov_count_++] = {const_cast<uint8_t *>(ZERO_FILL), len};
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
