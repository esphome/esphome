#include "api_overflow_buffer.h"
#ifdef USE_API
#include <algorithm>
#include <cstring>

namespace esphome::api {

ssize_t APIOverflowBuffer::try_drain(socket::Socket *socket) {
  // Nested call from inside socket->write(); see draining_
  if (this->draining_)
    return 0;

  // RAII so the flag is cleared on every return path
  struct DrainGuard {
    explicit DrainGuard(bool &flag) : flag_(flag) { flag_ = true; }
    ~DrainGuard() { this->flag_ = false; }
    bool &flag_;
  } guard(this->draining_);

  while (this->count_ > 0) {
    const uint8_t *msg = this->buf_.data() + this->head_;
    uint16_t len;
    std::memcpy(&len, msg, LEN_PREFIX);
    const uint16_t remaining = len - this->front_sent_;

    ssize_t sent = socket->write(msg + LEN_PREFIX + this->front_sent_, remaining);
    if (sent <= 0) {
      // -1 = error (caller checks errno for EWOULDBLOCK vs hard error)
      // 0 = nothing sent (treat as no progress)
      return sent;
    }
    if (static_cast<uint16_t>(sent) < remaining) {
      this->front_sent_ += static_cast<uint16_t>(sent);
      return sent;
    }
    this->head_ += LEN_PREFIX + len;
    this->front_sent_ = 0;
    this->count_--;
  }

  // Fully drained: rewind, keeping the capacity for the next stall unless a
  // release was requested while data was still queued
  this->head_ = 0;
  if (this->release_when_drained_) {
    this->release_when_drained_ = false;
    this->buf_.release();
  } else {
    this->buf_.clear();
  }
  return 0;
}

bool APIOverflowBuffer::enqueue_iov(const struct iovec *iov, int iovcnt, uint16_t total_len, uint16_t skip) {
  if (this->count_ >= API_MAX_SEND_QUEUE)
    return false;

  const uint16_t new_len = total_len - skip;
  size_t size = this->buf_.size();
  size_t want = size + LEN_PREFIX + new_len;

  if (this->head_ > 0 && want > this->buf_.capacity()) {
    // Slide the unsent bytes to the front so growth only copies live data
    const size_t live = size - this->head_;
    std::memmove(this->buf_.data(), this->buf_.data() + this->head_, live);
    this->head_ = 0;
    (void) this->buf_.resize(live);  // live <= capacity, cannot fail
    size = live;
    want = live + LEN_PREFIX + new_len;
  }
  // Offsets are 16 bit, so the backlog is capped at 64 KB per connection
  if (want > UINT16_MAX)
    return false;

  const size_t reserve = std::min<size_t>((want + GROW_QUANTUM - 1) & ~(GROW_QUANTUM - 1), UINT16_MAX);
  if (!this->buf_.reserve_and_resize(reserve, want))
    return false;

  uint8_t *dst = this->buf_.data() + size;
  std::memcpy(dst, &new_len, LEN_PREFIX);
  dst += LEN_PREFIX;
  uint16_t to_skip = skip;
  for (int i = 0; i < iovcnt; i++) {
    if (to_skip >= iov[i].iov_len) {
      to_skip -= static_cast<uint16_t>(iov[i].iov_len);
    } else {
      const uint8_t *src = reinterpret_cast<uint8_t *>(iov[i].iov_base) + to_skip;
      uint16_t len = static_cast<uint16_t>(iov[i].iov_len) - to_skip;
      std::memcpy(dst, src, len);
      dst += len;
      to_skip = 0;
    }
  }

  // Publish only after the copy completes so a half-built message is never sent
  this->count_++;
  return true;
}

}  // namespace esphome::api

#endif  // USE_API
