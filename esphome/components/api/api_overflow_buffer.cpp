#include "api_overflow_buffer.h"
#ifdef USE_API
#include <cstring>

namespace esphome::api {

ssize_t APIOverflowBuffer::try_drain(socket::Socket *socket) {
  // Nested call from inside socket->write(); see draining_
  if (this->draining_)
    return 0;

  struct DrainGuard {
    APIOverflowBuffer &owner;
    ~DrainGuard() { this->owner.draining_ = false; }
  } guard{*this};
  this->draining_ = true;

  while (this->count_ > 0) {
    uint8_t *msg = this->buf_.data() + this->head_;
    size_t len = msg[0] | (msg[1] << 8);

    ssize_t sent = socket->write(msg + LEN_PREFIX, len);
    if (sent <= 0)
      return sent;
    if (static_cast<size_t>(sent) < len) {
      // Step past the sent bytes and rewrite the prefix there; it lands on bytes already sent
      this->head_ += sent;
      len -= sent;
      msg += sent;
      msg[0] = len;
      msg[1] = len >> 8;
      return sent;
    }
    this->head_ += LEN_PREFIX + len;
    this->count_--;
  }

  this->head_ = 0;
  if (this->release_when_drained_) {
    this->release_when_drained_ = false;
    this->buf_.release();
  } else {
    this->buf_.clear();
  }
  return 0;
}

bool APIOverflowBuffer::enqueue_iov(const struct iovec *iov, int iovcnt, size_t total_len, size_t skip) {
  if (this->count_ >= API_MAX_SEND_QUEUE)
    return false;

  const size_t new_len = total_len - skip;
  const size_t new_bytes = LEN_PREFIX + new_len;
  const size_t live = this->buf_.size() - this->head_;
  // A lone message is only bound by the buffer; refusing it would just drop the connection
  if (live + new_bytes > (this->count_ > 0 ? MAX_BYTES : MAX_LONE_BYTES))
    return false;

  if (this->buf_.size() + new_bytes > this->buf_.capacity()) {
    // Storage would move under an outer drain's write()
    if (this->draining_)
      return false;
    if (this->head_ > 0) {
      // Reclaim the sent prefix before growing
      this->buf_.drop_front(this->head_);
      this->head_ = 0;
    }
    if (!this->buf_.reserve(reserve_for(live + new_bytes)))
      return false;
  }

  uint8_t *dst = this->buf_.append(new_bytes);
  if (dst == nullptr)
    return false;
  dst[0] = new_len;
  dst[1] = new_len >> 8;
  dst += LEN_PREFIX;
  for (const struct iovec *end = iov + iovcnt; iov != end; iov++) {
    if (skip >= iov->iov_len) {
      skip -= iov->iov_len;
    } else {
      const size_t len = iov->iov_len - skip;
      std::memcpy(dst, static_cast<const uint8_t *>(iov->iov_base) + skip, len);
      dst += len;
      skip = 0;
    }
  }

  this->count_++;
  return true;
}

}  // namespace esphome::api

#endif  // USE_API
