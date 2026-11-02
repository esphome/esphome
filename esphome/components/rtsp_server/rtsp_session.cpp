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

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

static constexpr uint32_t RTP_CLOCK_RATE = 90000;
static constexpr uint32_t SESSION_TIMEOUT_MS = 60000;
static constexpr char REQUEST_END[] = "\r\n\r\n";
static constexpr size_t REQUEST_END_LEN = sizeof(REQUEST_END) - 1;
static constexpr uint8_t ZERO_FILL[64]{};
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

}  // namespace

void RTSPSession::open(std::unique_ptr<socket::Socket> socket, uint32_t now, camera_video::EncodedVideoStream *stream,
                       uint16_t payload_size) {
  this->stream_ = stream;
  this->payload_size_ = payload_size;
  this->packetizer_ = make_rtp_packetizer(stream, payload_size);
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
  this->cseq_ = 0;
  this->last_frame_ms_ = 0;
  this->last_activity_ms_ = now;
  this->ssrc_ = random_uint32();
  this->sequence_number_ = static_cast<uint16_t>(random_uint32());
  this->rtp_timestamp_ = random_uint32();
  this->timestamp_origin_us_ = 0;
  this->timestamp_origin_rtp_ = this->rtp_timestamp_;
  if (this->packetizer_ != nullptr)
    this->packetizer_->reset();
  format_hex_to(this->session_id_, random_uint32());
}

void RTSPSession::close_() {
  this->packetizer_.reset();
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
    if (this->packetizer_ == nullptr || !this->packetizer_->write_sdp(this->sdp_buf_.data(), this->sdp_buf_.size())) {
      this->queue_response_(503, "Service Unavailable");
      return;
    }
    const char *sdp = this->sdp_buf_.data();
    const size_t sdp_length = std::strlen(sdp);
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
  if (this->packetizer_ != nullptr)
    this->packetizer_->reset();
  if (this->stream_ != nullptr)
    this->stream_->request_keyframe();
  this->queue_response_(200, "OK", "Range: npt=0.000-\r\n");
}

void RTSPSession::stop_playing_() {
  this->state_ = State::READY;
  if (this->packetizer_ != nullptr)
    this->packetizer_->reset();
}

void RTSPSession::on_encoded_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame, uint32_t now) {
  if (this->state_ != State::PLAYING || frame == nullptr || this->packetizer_ == nullptr)
    return;
  if (this->has_frame_() || this->iov_pos_ < this->iov_count_ || this->filler_len_ > 0) {
    this->packetizer_->discontinuity();
    return;
  }
  if (!this->packetizer_->set_frame(frame))
    return;
  uint64_t timestamp_us = frame->get_timestamp_us();
  if (timestamp_us == 0)
    timestamp_us = static_cast<uint64_t>(now) * 1000ULL;
  if (this->timestamp_origin_us_ == 0 || timestamp_us < this->timestamp_origin_us_) {
    this->timestamp_origin_us_ = timestamp_us;
    this->timestamp_origin_rtp_ = this->rtp_timestamp_;
  }
  this->rtp_timestamp_ =
      this->timestamp_origin_rtp_ +
      static_cast<uint32_t>((timestamp_us - this->timestamp_origin_us_) * RTP_CLOCK_RATE / 1000000ULL);
  this->last_frame_ms_ = now;
  this->frame_progress_ms_ = now;
}

void RTSPSession::on_discontinuity() {
  if (this->packetizer_ != nullptr)
    this->packetizer_->discontinuity();
}

bool RTSPSession::has_frame_() const { return this->packetizer_ != nullptr && this->packetizer_->has_frame(); }

bool RTSPSession::queue_next_packet_() {
  if (this->packetizer_ == nullptr || !this->packetizer_->has_frame())
    return false;
  RtpPacketContext ctx{reinterpret_cast<uint8_t *>(this->send_buf_.data()),
                       this->iov_,
                       this->iov_count_,
                       this->sequence_number_,
                       this->rtp_timestamp_,
                       this->ssrc_};
  if (!this->packetizer_->next_packet(ctx))
    return false;
  this->iov_pos_ = 0;
  this->sending_packet_ = true;
  return true;
}

void RTSPSession::drop_frame_() {
  if (this->packetizer_ != nullptr && this->packetizer_->has_frame()) {
    this->packetizer_->discontinuity();
    this->packetizer_->reset();
  }
  if (!this->sending_packet_ || this->iov_pos_ >= this->iov_count_)
    return;
  // Even an unwritten packet is kept, so output stays pending and on_encoded_frame() takes nothing until the
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
