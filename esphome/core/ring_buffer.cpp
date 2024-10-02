#include "ring_buffer.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "helpers.h"

namespace esphome {

static const char *const TAG = "ring_buffer";

RingBuffer::~RingBuffer() {
  if (this->handle_ != nullptr) {
    vStreamBufferDelete(this->handle_);
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    allocator.deallocate(this->storage_, this->size_);
  }
}

std::unique_ptr<RingBuffer> RingBuffer::create(size_t len) {
  std::unique_ptr<RingBuffer> rb = make_unique<RingBuffer>();

  rb->size_ = len + 1;

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  rb->storage_ = allocator.allocate(rb->size_);
  if (rb->storage_ == nullptr) {
    return nullptr;
  }

  rb->handle_ = xStreamBufferCreateStatic(rb->size_, 1, rb->storage_, &rb->structure_);
  ESP_LOGD(TAG, "Created ring buffer with size %u", len);
  return rb;
}

size_t RingBuffer::read(void *data, size_t len, TickType_t ticks_to_wait) {
  if (ticks_to_wait > 0)
    xStreamBufferSetTriggerLevel(this->handle_, len);

  size_t bytes_read = xStreamBufferReceive(this->handle_, data, len, ticks_to_wait);

  xStreamBufferSetTriggerLevel(this->handle_, 1);

  return bytes_read;
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

size_t RingBuffer::write_without_replacement(void *data, size_t len, TickType_t ticks_to_wait) {
  return xStreamBufferSend(this->handle_, data, len, ticks_to_wait);
}

size_t RingBuffer::available() const { return xStreamBufferBytesAvailable(this->handle_); }

size_t RingBuffer::free() const { return xStreamBufferSpacesAvailable(this->handle_); }

BaseType_t RingBuffer::reset() { return xStreamBufferReset(this->handle_); }

}  // namespace esphome

#endif
