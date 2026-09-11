#include "api_overflow_buffer.h"
#ifdef USE_API
#include <cstring>

namespace esphome::api {

ssize_t APIOverflowBuffer::try_drain(socket::Socket *socket) {
  // Nested call from inside socket->write(); see draining_
  if (this->draining_)
    return 0;

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
    if (sent <= 0)
      return sent;
    if (sent < remaining) {
      this->front_sent_ += static_cast<uint16_t>(sent);
      return sent;
    }
    this->head_ += LEN_PREFIX + len;
    this->front_sent_ = 0;
    this->count_--;
  }

  // Fully drained: rewind; keep the capacity unless release() asked otherwise
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
  // An empty backlog always takes one message: refusing it drops the connection,
  // and a lone message costs no more than the old per-message allocation
  if (size - this->head_ + LEN_PREFIX + new_len > (this->count_ > 0 ? MAX_BYTES : MAX_SINGLE_BYTES))
    return false;

  if (size + LEN_PREFIX + new_len > this->buf_.capacity()) {
    // Storage would move; not under an outer drain whose write() still points into it
    if (this->draining_)
      return false;
    if (this->head_ > 0) {
      // Slide the unsent bytes to the front so growth only copies live data
      size -= this->head_;
      std::memmove(this->buf_.data(), this->buf_.data() + this->head_, size);
      this->head_ = 0;
      (void) this->buf_.resize(size);  // keeps size() consistent if the grow below fails
    }
  }

  const size_t want = size + LEN_PREFIX + new_len;
  const size_t reserve = std::min((want + GROW_QUANTUM - 1) & ~(GROW_QUANTUM - 1), MAX_SINGLE_BYTES);
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
      const uint8_t *src = static_cast<const uint8_t *>(iov[i].iov_base) + to_skip;
      uint16_t len = static_cast<uint16_t>(iov[i].iov_len) - to_skip;
      std::memcpy(dst, src, len);
      dst += len;
      to_skip = 0;
    }
  }

  // Publish after the copy so a half-built message is never sent
  this->count_++;
  return true;
}

}  // namespace esphome::api

#endif  // USE_API
