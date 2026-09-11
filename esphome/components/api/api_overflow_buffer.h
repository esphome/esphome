#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "esphome/core/defines.h"
#ifdef USE_API

#include "esphome/components/socket/headers.h"
#include "esphome/components/socket/socket.h"
#include "api_buffer.h"

namespace esphome::api {

/// TCP send backlog stored in one contiguous per-connection buffer.
///
/// Under normal operation this buffer is **never used** — data goes straight
/// from the frame helper to the socket.  It only fills when the LWIP TCP
/// send buffer is full (slow client, lossy link, heavy logging).  The
/// storage is allocated on the first stall and kept at its high-water mark,
/// so a link that keeps stalling does not cycle heap allocations.
///
/// Each queued message is stored as a 2 byte length prefix followed by its
/// bytes.  API_MAX_SEND_QUEUE bounds the number of queued messages and, at
/// 2 KB per slot, the number of queued bytes; exceeding either marks the
/// connection failed.
class APIOverflowBuffer {
 public:
  /// True when no backlogged data is waiting.
  bool empty() const { return this->count_ == 0; }

  /// Try to drain queued messages to the socket.
  /// Precondition: !empty().
  /// Returns bytes-written > 0 on success/partial, 0 for a re-entrant call,
  /// -1 on error (caller must check errno to distinguish EWOULDBLOCK from
  /// hard errors). Callers only need to act on -1.
  ssize_t try_drain(socket::Socket *socket);

  /// Enqueue unsent IOV data into the backlog.
  /// Copies iov data starting at byte offset `skip` as one queued message.
  /// Returns false if the queue is full, the byte limit is exceeded, or
  /// allocation fails (caller should fail the connection).
  bool enqueue_iov(const struct iovec *iov, int iovcnt, uint16_t total_len, uint16_t skip);

  /// Free the retained storage, now if empty, otherwise once it has drained.
  void release() {
    if (this->count_ == 0) {
      this->buf_.release();
    } else {
      this->release_when_drained_ = true;
    }
  }

 protected:
  static constexpr size_t LEN_PREFIX = 2;
  // Backlog byte limit; offsets are 16 bit so it never exceeds 64 KB
  static constexpr size_t MAX_BYTES = std::min<size_t>(API_MAX_SEND_QUEUE * 2048, UINT16_MAX);
  // Reserve in 256 byte steps so a creeping high-water mark settles after a
  // couple of allocations instead of one per new size.
  static constexpr size_t GROW_QUANTUM = 256;

  APIBuffer buf_;
  uint16_t head_{0};        // offset of the front message's length prefix; bytes before it are sent
  uint16_t front_sent_{0};  // bytes of the front message already written
  uint8_t count_{0};
  // Guards against re-entrant drains: socket->write() can re-enter the API
  // send path (e.g. a log message emitted from an lwip callback). A nested
  // drain reports no progress so the outer one keeps its bookkeeping intact.
  bool draining_{false};
  bool release_when_drained_{false};
};

}  // namespace esphome::api

#endif  // USE_API
