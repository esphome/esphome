#include "api_overflow_buffer.h"
#ifdef USE_API
#include <cstring>

namespace esphome::api {

APIOverflowBuffer::~APIOverflowBuffer() {
  for (auto *entry : this->queue_) {
    if (entry != nullptr)
      Entry::destroy(entry);
  }
}

ssize_t APIOverflowBuffer::try_drain(socket::Socket *socket) {
  while (this->count_ > 0) {
    Entry *front = this->queue_[this->head_];

    ssize_t sent = socket->write(front->current_data(), front->remaining());

    if (sent <= 0) {
      // -1 = error (caller checks errno for EWOULDBLOCK vs hard error)
      // 0 = nothing sent (treat as no progress)
      return sent;
    }

    if (static_cast<uint16_t>(sent) < front->remaining()) {
      // Partially sent, update offset and stop
      front->offset += static_cast<uint16_t>(sent);
      return sent;
    }

    // Entry fully sent — free it and advance
    Entry::destroy(front);
    this->queue_[this->head_] = nullptr;
    this->head_ = (this->head_ + 1) % API_MAX_SEND_QUEUE;
    this->count_--;
  }

  return 0;  // All drained
}

bool APIOverflowBuffer::enqueue_iov(const struct iovec *iov, int iovcnt, uint16_t total_len, uint16_t skip) {
  if (this->count_ >= API_MAX_SEND_QUEUE)
    return false;

  uint16_t buffer_size = total_len - skip;
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  auto *entry = new Entry{new uint8_t[buffer_size], buffer_size, 0};
  this->queue_[this->tail_] = entry;

  uint16_t to_skip = skip;
  uint16_t write_pos = 0;

  for (int i = 0; i < iovcnt; i++) {
    if (to_skip >= iov[i].iov_len) {
      to_skip -= static_cast<uint16_t>(iov[i].iov_len);
    } else {
      const uint8_t *src = reinterpret_cast<uint8_t *>(iov[i].iov_base) + to_skip;
      uint16_t len = static_cast<uint16_t>(iov[i].iov_len) - to_skip;
      std::memcpy(entry->data + write_pos, src, len);
      write_pos += len;
      to_skip = 0;
    }
  }

  this->tail_ = (this->tail_ + 1) % API_MAX_SEND_QUEUE;
  this->count_++;
  return true;
}

}  // namespace esphome::api

#endif  // USE_API
