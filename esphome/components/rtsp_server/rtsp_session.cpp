#include "rtsp_session.h"

#ifdef USE_CAMERA

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <utility>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

static constexpr uint8_t RTP_PAYLOAD_TYPE_JPEG = 26;
static constexpr uint32_t RTP_CLOCK_RATE = 90000;
static constexpr uint32_t SESSION_TIMEOUT_MS = 60000;
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

}  // namespace

/// Scans JPEG marker segments (SOF0/1, DQT, SOS) for the dimensions, quantization tables and the start of
/// the entropy-coded scan data. Assumes the standard Huffman tables (true for esp32-camera), so DHT segments
/// are not parsed; receivers rebuild them per RFC 2435. Restart intervals (DRI) are not handled.
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
      if (seg_len < 8)
        return false;
      out.height = encode_uint16(data[seg_start + 1], data[seg_start + 2]);
      out.width = encode_uint16(data[seg_start + 3], data[seg_start + 4]);
      uint8_t num_components = data[seg_start + 5];
      if (num_components >= 1 && seg_start + 7 < seg_end) {
        uint8_t sampling = data[seg_start + 7];  // Y component H/V sampling factors
        out.type = (sampling == 0x22) ? 1 : 0;   // 2x2 -> 4:2:0, otherwise assume 4:2:2
      }
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
  this->should_close_ = false;
  this->close_after_send_ = false;
  this->request_len_ = 0;
  this->cseq_ = 0;
  this->last_frame_ms_ = 0;
  this->last_activity_ms_ = now;
  this->ssrc_ = random_uint32();
  format_hex_to(this->session_id_, random_uint32());
}

void RTSPSession::close_() {
  this->frame_.reset();
  this->socket_.reset();
  this->state_ = State::INIT;
  this->iov_pos_ = 0;
  this->iov_count_ = 0;
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
  if (!this->socket_->ready())
    return;

  const size_t buf_capacity = this->request_buf_.size() - 1;
  while (true) {
    if (this->request_len_ >= buf_capacity) {
      ESP_LOGW(TAG, "Request too large, closing session");
      this->should_close_ = true;
      return;
    }
    ssize_t n = this->socket_->read(this->request_buf_.data() + this->request_len_, buf_capacity - this->request_len_);
    if (n > 0) {
      this->request_len_ += static_cast<uint16_t>(n);
      this->last_activity_ms_ = now;
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

  this->handle_request_(StringRef(buf.c_str(), terminator), port);
  // Bytes pipelined after this request are discarded; RTSP clients wait for each response.
  this->request_len_ = 0;
}

void RTSPSession::handle_request_(StringRef request, uint16_t port) {
  size_t line_end = request.find("\r\n");
  StringRef first_line = line_end == std::string::npos ? request : StringRef(request.c_str(), line_end);

  this->cseq_ = 0;
  size_t sp1 = first_line.find(' ');
  size_t sp2 = sp1 == std::string::npos ? std::string::npos : first_line.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) {
    this->queue_response_(400, "Bad Request");
    return;
  }
  StringRef method(first_line.c_str(), sp1);
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
      if (str_equals_case_insensitive(key, StringRef::from_lit("CSeq"))) {
        this->cseq_ = static_cast<uint32_t>(stol(value));
      } else if (str_equals_case_insensitive(key, StringRef::from_lit("Transport"))) {
        transport = value;
      }
    }

    if (next == std::string::npos)
      break;
    pos = next + 2;
  }

  if (str_equals_case_insensitive(method, StringRef::from_lit("OPTIONS"))) {
    this->queue_response_(200, "OK", "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER\r\n");
  } else if (str_equals_case_insensitive(method, StringRef::from_lit("DESCRIBE"))) {
    char addr_buf[socket::SOCKADDR_STR_LEN];
    this->socket_->getsockname_to(addr_buf);
    this->queue_response_(200, "OK",
                          "Content-Base: rtsp://%s:%u/\r\n"
                          "Content-Type: application/sdp\r\n"
                          "Content-Length: %u\r\n",
                          addr_buf, port, static_cast<unsigned>(sizeof(SDP_BODY) - 1));
    this->iov_[this->iov_count_++] = {const_cast<char *>(SDP_BODY), sizeof(SDP_BODY) - 1};
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
}

void RTSPSession::handle_setup_(StringRef transport_header) {
  if (transport_header.find("RTP/AVP/TCP") == std::string::npos) {
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
  this->sequence_number_ = static_cast<uint16_t>(random_uint32());
  this->rtp_timestamp_ = random_uint32();
  this->frame_.reset();  // a repeated PLAY may arrive mid-frame
  this->queue_response_(200, "OK", "Range: npt=0.000-\r\n");
}

void RTSPSession::stop_playing_() {
  this->state_ = State::READY;
  this->frame_.reset();
}

void RTSPSession::on_new_frame(const std::shared_ptr<camera::CameraImage> &image, const JpegInfo &jpeg, uint32_t now) {
  if (this->state_ != State::PLAYING || this->frame_ != nullptr)
    return;
  this->frame_ = image;
  this->jpeg_ = jpeg;
  this->frame_sent_ = 0;

  uint32_t elapsed_ms = this->last_frame_ms_ == 0 ? 0 : now - this->last_frame_ms_;
  this->rtp_timestamp_ += static_cast<uint32_t>((static_cast<uint64_t>(elapsed_ms) * RTP_CLOCK_RATE) / 1000);
  this->last_frame_ms_ = now;
}

bool RTSPSession::queue_next_packet_() {
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
  this->frame_sent_ += payload_len;
  return true;
}

bool RTSPSession::flush_(uint32_t now) {
  if (this->should_close_)
    return false;
  while (this->iov_pos_ < this->iov_count_) {
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
  return true;
}

}  // namespace esphome::rtsp_server

#endif
