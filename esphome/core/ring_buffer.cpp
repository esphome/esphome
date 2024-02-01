#include "ring_buffer.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "helpers.h"

namespace esphome {

static const char *const TAG = "ring_buffer";

std::unique_ptr<RingBuffer> RingBuffer::create(size_t len) {
  std::unique_ptr<RingBuffer> rb = make_unique<RingBuffer>();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  rb->storage_ = allocator.allocate(len + 1);
  if (rb->storage_ == nullptr) {
    return nullptr;
  }

  rb->handle_ = xStreamBufferCreateStatic(len + 1, 0, rb->storage_, &rb->structure_);
  ESP_LOGD(TAG, "Created ring buffer with size %u", len);
  return rb;
}

size_t RingBuffer::read(void *data, size_t len, TickType_t ticks_to_wait) {
  return xStreamBufferReceive(this->handle_, data, len, ticks_to_wait);
}

size_t RingBuffer::write(void *data, size_t len) {
  size_t free = this->free();
  if (free < len) {
    size_t needed = len - free;
    uint8_t discard[needed];
    xStreamBufferReceive(this->handle_, discard, needed, 0);
  }
  return xStreamBufferSend(this->handle_, data, len, 0);
}

size_t RingBuffer::available() const { return xStreamBufferBytesAvailable(this->handle_); }

size_t RingBuffer::free() const { return xStreamBufferSpacesAvailable(this->handle_); }

BaseType_t RingBuffer::reset() { return xStreamBufferReset(this->handle_); }

}  // namespace esphome

#endif
