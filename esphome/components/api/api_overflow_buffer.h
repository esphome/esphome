#pragma once
#include <array>
#include <cstdint>
#include <sys/types.h>

#include "esphome/core/defines.h"
#ifdef USE_API

#include "esphome/components/socket/headers.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/helpers.h"

namespace esphome::api {

/// Circular queue of heap-allocated byte buffers used as a TCP send backlog.
///
/// Under normal operation this buffer is **never used** — data goes straight
/// from the frame helper to the socket.  It only fills when the LWIP TCP
/// send buffer is full (slow client, congested network, heavy logging).
/// The queue drains automatically on subsequent write/loop calls once the
/// socket becomes writable again.
///
/// Capacity is compile-time-fixed via API_MAX_SEND_QUEUE (set from Python
/// config).  If the queue fills completely the connection is marked failed.
class APIOverflowBuffer {
 public:
  /// A single heap-allocated send-backlog entry.
  /// Lifetime is manually managed — see destroy().
  struct Entry {
    uint8_t *data;
    uint16_t size;    // Total size of the buffer
    uint16_t offset;  // Current send offset within the buffer

    uint16_t remaining() const { return this->size - this->offset; }
    const uint8_t *current_data() const { return this->data + this->offset; }

    /// Free this entry and its data buffer.
    static ESPHOME_ALWAYS_INLINE void destroy(Entry *entry) {
      delete[] entry->data;
      delete entry;  // NOLINT(cppcoreguidelines-owning-memory)
    }
  };

  ~APIOverflowBuffer();

  /// True when no backlogged data is waiting.
  bool empty() const { return this->count_ == 0; }

  /// True when the queue has no room for another entry.
  bool full() const { return this->count_ >= API_MAX_SEND_QUEUE; }

  /// Number of entries currently queued.
  uint8_t count() const { return this->count_; }

  /// Try to drain queued data to the socket.
  /// Returns bytes-written > 0 on success/partial, 0 if all drained or no progress,
  /// -1 on error (caller must check errno to distinguish EWOULDBLOCK from hard errors).
  /// Callers only need to act on -1; 0 and positive values both mean "no error".
  /// Frees entries as they are fully sent.
  ssize_t try_drain(socket::Socket *socket);

  /// Enqueue unsent IOV data into the backlog.
  /// Copies iov data starting at byte offset `skip` into a new entry.
  /// Returns false if the queue is full (caller should fail the connection).
  bool enqueue_iov(const struct iovec *iov, int iovcnt, uint16_t total_len, uint16_t skip);

 protected:
  std::array<Entry *, API_MAX_SEND_QUEUE> queue_{};
  uint8_t head_{0};
  uint8_t tail_{0};
  uint8_t count_{0};
};

}  // namespace esphome::api

#endif  // USE_API
