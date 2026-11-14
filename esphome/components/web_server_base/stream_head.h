#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <strings.h>

#include "esphome/core/defines.h"
#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

// Pure helpers of AsyncStreamResponse, kept free of httpd and lwIP so they build on the host.
// The iovec type is a template parameter: lwIP defines its own on the device, POSIX on the host.

namespace esphome::web_server_base {

// The stream helpers take strings and pieces that may sit in flash on ESP8266: only word reads
// work there, so copies and string scans go through the _P functions (which also read RAM)
#ifdef USE_ESP8266
#define STREAM_PSTR(s) PSTR(s)
inline size_t stream_strlen(const char *s) { return strlen_P(s); }
inline int stream_strcasecmp(const char *ram, const char *any) { return strcasecmp_P(ram, any); }
inline void stream_memcpy(void *dst, const void *src, size_t len) { memcpy_P(dst, src, len); }
#else
#define STREAM_PSTR(s) (s)
inline size_t stream_strlen(const char *s) { return strlen(s); }
inline int stream_strcasecmp(const char *ram, const char *any) { return strcasecmp(ram, any); }
inline void stream_memcpy(void *dst, const void *src, size_t len) { std::memcpy(dst, src, len); }
#endif

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

/// True if headers holds a header named name, compared case insensitively
inline bool has_header(std::span<const HttpHeader> headers, const char *name) {
  for (const auto &header : headers) {
    if (stream_strcasecmp(name, header.name) == 0) {
      return true;
    }
  }
  return false;
}

/// Gathers the head of a stream response: "HTTP/1.1 <status>", Content-Type, headers, the default_headers that
/// headers does not replace, the blank line and body (may be null). send(Iovec *iov, int count) gets each full batch of
/// STREAM_HEAD_MAX_IOV pieces and then the rest; once it returns false the later pieces are skipped. Returns false if
/// any send failed.
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
    void add(const char *str) { this->add(str, stream_strlen(str)); }
    void add_header(const HttpHeader &header) {
      this->add(STREAM_PSTR("\r\n"), 2);
      this->add(header.name);
      this->add(STREAM_PSTR(": "), 2);
      this->add(header.value);
    }
  } batch{send};

  batch.add(STREAM_PSTR("HTTP/1.1 "), 9);
  batch.add(status);
  batch.add(STREAM_PSTR("\r\nContent-Type: "), 16);
  batch.add(content_type);
  for (const auto &header : headers) {
    batch.add_header(header);
  }
  for (const auto &header : default_headers) {
    if (!has_header(headers, header.name)) {
      batch.add_header(header);
    }
  }
  batch.add(STREAM_PSTR("\r\n\r\n"), 4);
  if (body != nullptr) {
    batch.add(body);
  }
  batch.flush();
  return batch.ok;
}

/// AsyncClient::add() flags; the three TCP libraries use the same values (checked where they are included)
static constexpr uint8_t STREAM_ADD_FLAG_COPY = 0x01;
static constexpr uint8_t STREAM_ADD_FLAG_MORE = 0x02;

/// Queues a gather list on an AsyncClient-like client through scratch, so each add() carries up to scratch_size bytes
/// (on LibreTiny every add() is a round trip into the tcpip thread). Stops at the first add() that takes less than
/// offered, calls send() once if anything was taken, and returns the bytes taken.
template<typename Client, typename Iovec>
size_t add_iovecs(Client &client, const Iovec *iov, int count, char *scratch, size_t scratch_size) {
  size_t taken = 0;
  size_t fill = 0;
  int i = 0;
  size_t offset = 0;  // bytes of iov[i] already copied
  while (i < count) {
    const size_t piece = std::min(iov[i].iov_len - offset, scratch_size - fill);
    stream_memcpy(scratch + fill, static_cast<const char *>(iov[i].iov_base) + offset, piece);
    fill += piece;
    offset += piece;
    if (offset == iov[i].iov_len) {
      ++i;
      offset = 0;
    }
    if (fill < scratch_size && i < count) {
      continue;
    }
    const uint8_t flags = STREAM_ADD_FLAG_COPY | (i < count ? STREAM_ADD_FLAG_MORE : 0);
    const size_t added = client.add(scratch, fill, flags);
    taken += added;
    if (added < fill) {
      break;
    }
    fill = 0;
  }
  if (taken != 0) {
    client.send();
  }
  return taken;
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

}  // namespace esphome::web_server_base
