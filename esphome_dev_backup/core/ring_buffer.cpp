#include "ring_buffer.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "helpers.h"

namespace esphome {

static const char *const TAG = "ring_buffer";

RingBuffer::~RingBuffer() {
  if (this->handle_ != nullptr) {
    vRingbufferDelete(this->handle_);
    RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOW_FAILURE);
    allocator.deallocate(this->storage_, this->size_);
  }
}

std::unique_ptr<RingBuffer> RingBuffer::create(size_t len) {
  std::unique_ptr<RingBuffer> rb = make_unique<RingBuffer>();

  rb->size_ = len;

  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOW_FAILURE);
  rb->storage_ = allocator.allocate(rb->size_);
  if (rb->storage_ == nullptr) {
    return nullptr;
  }

  rb->handle_ = xRingbufferCreateStatic(rb->size_, RINGBUF_TYPE_BYTEBUF, rb->storage_, &rb->structure_);
  ESP_LOGD(TAG, "Created ring buffer with size %u", len);

  return rb;
}

size_t RingBuffer::read(void *data, size_t len, TickType_t ticks_to_wait) {
  size_t bytes_read = 0;

  void *buffer_data = xRingbufferReceiveUpTo(this->handle_, &bytes_read, ticks_to_wait, len);

  if (buffer_data == nullptr) {
    return 0;
  }

  std::memcpy(data, buffer_data, bytes_read);

  vRingbufferReturnItem(this->handle_, buffer_data);

  if (bytes_read < len) {
    // Data may have wrapped around, so read a second time to receive the remainder
    size_t follow_up_bytes_read = 0;
    size_t bytes_remaining = len - bytes_read;

    buffer_data = xRingbufferReceiveUpTo(this->handle_, &follow_up_bytes_read, 0, bytes_remaining);

    if (buffer_data == nullptr) {
      return bytes_read;
    }

    std::memcpy((void *) ((uint8_t *) (data) + bytes_read), buffer_data, follow_up_bytes_read);

    vRingbufferReturnItem(this->handle_, buffer_data);
    bytes_read += follow_up_bytes_read;
  }

  return bytes_read;
}

size_t RingBuffer::write(const void *data, size_t len) {
  size_t free = this->free();
  if (free < len) {
    // Free enough space in the ring buffer to fit the new data
    this->discard_bytes_(len - free);
  }
  return this->write_without_replacement(data, len, 0);
}

size_t RingBuffer::write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait) {
  if (!xRingbufferSend(this->handle_, data, len, ticks_to_wait)) {
    // Couldn't fit all the data, so only write what will fit
    size_t free = std::min(this->free(), len);
    if (xRingbufferSend(this->handle_, data, free, 0)) {
      return free;
    }
    return 0;
  }
  return len;
}

size_t RingBuffer::available() const {
  UBaseType_t ux_items_waiting = 0;
  vRingbufferGetInfo(this->handle_, nullptr, nullptr, nullptr, nullptr, &ux_items_waiting);
  return ux_items_waiting;
}

size_t RingBuffer::free() const { return xRingbufferGetCurFreeSize(this->handle_); }

BaseType_t RingBuffer::reset() {
  // Discards all the available data
  return this->discard_bytes_(this->available());
}

bool RingBuffer::discard_bytes_(size_t discard_bytes) {
  size_t bytes_read = 0;

  void *buffer_data = xRingbufferReceiveUpTo(this->handle_, &bytes_read, 0, discard_bytes);
  if (buffer_data != nullptr)
    vRingbufferReturnItem(this->handle_, buffer_data);

  if (bytes_read < discard_bytes) {
    size_t wrapped_bytes_read = 0;
    buffer_data = xRingbufferReceiveUpTo(this->handle_, &wrapped_bytes_read, 0, discard_bytes - bytes_read);
    if (buffer_data != nullptr) {
      vRingbufferReturnItem(this->handle_, buffer_data);
      bytes_read += wrapped_bytes_read;
    }
  }

  return (bytes_read == discard_bytes);
}

}  // namespace esphome

#endif
