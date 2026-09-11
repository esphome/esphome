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

/// TCP send backlog, only used when the socket send buffer is full.
/// One contiguous buffer per connection, allocated on the first stall and
/// kept at its high-water mark so a lossy link does not churn the heap.
/// Messages are stored as a 2 byte length prefix plus payload.
/// API_MAX_SEND_QUEUE bounds queued messages and, at 2 KB per slot, queued
/// bytes; exceeding either fails the connection.
class APIOverflowBuffer {
 public:
  /// True when no backlogged data is waiting.
  bool empty() const { return this->count_ == 0; }

  /// Drain queued messages to the socket. Must not be called while empty().
  /// Returns bytes written, 0 for a re-entrant call, -1 on error (check errno
  /// for EWOULDBLOCK); callers only need to act on -1.
  ssize_t try_drain(socket::Socket *socket);

  /// Queue iov data from byte offset `skip` as one message.
  /// Returns false when a limit is hit, allocation fails, or storage would move
  /// during a drain; the caller should fail the connection.
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
  static constexpr size_t BYTES_PER_SLOT = 2048;
  static constexpr size_t MAX_BYTES = std::min(API_MAX_SEND_QUEUE * BYTES_PER_SLOT, APIBuffer::MAX_SIZE);
  // Reserve in 256 byte steps so a creeping high-water mark settles quickly
  static constexpr size_t GROW_QUANTUM = 256;
  static constexpr size_t reserve_for(size_t want) {
    return std::min((want + GROW_QUANTUM - 1) & ~(GROW_QUANTUM - 1), APIBuffer::MAX_SIZE);
  }

  APIBuffer buf_;
  uint16_t head_{0};  // offset of the front message's length prefix; bytes before it are sent
  uint8_t count_{0};
  // socket->write() can re-enter the send path (log from an lwip callback):
  // a nested drain makes no progress and a nested enqueue never moves storage
  bool draining_ : 1 {false};
  bool release_when_drained_ : 1 {false};
};

}  // namespace esphome::api

#endif  // USE_API
