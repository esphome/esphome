#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

// Pure helpers of AsyncStreamResponse, kept free of httpd and lwIP so they build on the host.
// The iovec type is a template parameter: lwIP defines its own on the device, POSIX on the host.

namespace esphome::web_server_idf {

struct HttpHeader {
  const char *name;
  const char *value;
};

/// Drops the first sent bytes from a gather list after a write; a short write resumes inside the
/// element it stopped in. count reaches 0 once everything went out.
template<typename Iovec> void advance_iovecs(Iovec *&iov, int &count, size_t sent) {
  while (count > 0 && sent >= iov->iov_len) {
    sent -= iov->iov_len;
    ++iov;
    --count;
  }
  if (count > 0) {
    iov->iov_base = static_cast<char *>(iov->iov_base) + sent;
    iov->iov_len -= sent;
  }
}

/// Pieces in a head: 4 for the status line and Content-Type, 4 per header, the blank line and the body
constexpr size_t stream_head_iov_count(size_t header_count) { return 4 + 4 * header_count + 2; }

/// Pieces per batch: a head with up to four headers goes out in one write; 192 bytes on 32-bit
static constexpr int STREAM_HEAD_MAX_IOV = 24;

/// Gathers the head of a stream response: "HTTP/1.1 <status>", Content-Type, headers, default_headers, the
/// blank line and body (may be null). send(Iovec *iov, int count) gets each full batch of STREAM_HEAD_MAX_IOV pieces
/// and then the rest; once it returns false the later pieces are skipped. Returns false if any send failed.
template<typename Iovec, typename Send>
bool write_stream_head(Send &&send, const char *status, const char *content_type, std::span<const HttpHeader> headers,
                       std::span<const HttpHeader> default_headers, const char *body) {
  struct Batch {
    Send &send;
    int count{0};
    bool ok{true};
    Iovec iov[STREAM_HEAD_MAX_IOV];  // left uninitialized on purpose

    void flush() {
      if (this->ok && this->count > 0) {
        this->ok = this->send(this->iov, this->count);
      }
      this->count = 0;
    }
    void add(const char *buf, size_t len) {
      if (this->count == STREAM_HEAD_MAX_IOV) {
        this->flush();
      }
      this->iov[this->count++] = {const_cast<char *>(buf), len};
    }
    void add(const char *str) { this->add(str, strlen(str)); }
    void add_header(const HttpHeader &header) {
      this->add("\r\n", 2);
      this->add(header.name);
      this->add(": ", 2);
      this->add(header.value);
    }
  } batch{send};

  batch.add("HTTP/1.1 ", 9);
  batch.add(status);
  batch.add("\r\nContent-Type: ", 16);
  batch.add(content_type);
  for (const auto &header : headers) {
    batch.add_header(header);
  }
  for (const auto &header : default_headers) {
    batch.add_header(header);
  }
  batch.add("\r\n\r\n", 4);
  if (body != nullptr) {
    batch.add(body);
  }
  batch.flush();
  return batch.ok;
}

/// How long sends have made no progress. Zero means not running, so a start at millis() == 0 records 1.
/// The comparison is wraparound safe across a millis() rollover.
class StallClock {
 public:
  /// Starts the clock; a running clock keeps its start time.
  void start(uint32_t now) {
    if (this->started_ms_ == 0) {
      this->started_ms_ = now != 0 ? now : 1;
    }
  }
  void clear() { this->started_ms_ = 0; }
  bool is_running() const { return this->started_ms_ != 0; }
  /// True once the clock has run for timeout_ms
  bool is_expired(uint32_t now, uint32_t timeout_ms) const {
    return this->started_ms_ != 0 && static_cast<int32_t>(now - (this->started_ms_ + timeout_ms)) >= 0;
  }
  uint32_t elapsed(uint32_t now) const { return now - this->started_ms_; }

 protected:
  uint32_t started_ms_{0};
};

}  // namespace esphome::web_server_idf
