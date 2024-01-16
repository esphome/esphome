#include "ring_buffer.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "helpers.h"

namespace esphome {

static const char *TAG = "ring_buffer";

RingBuffer *RingBuffer::create(size_t size) {
  RingBuffer *rb = new RingBuffer();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  rb->storage = allocator.allocate(size);
  if (rb->storage == nullptr) {
    return nullptr;
  }

  rb->handle_ = xStreamBufferCreateStatic(size, 0, rb->storage, &rb->structure);
  return rb;
}

size_t RingBuffer::read(void *data, size_t size, TickType_t ticks_to_wait) {
  return xStreamBufferReceive(this->handle_, data, size, ticks_to_wait);
}

size_t RingBuffer::write(void *data, size_t size) {
  size_t free = this->free();
  if (free < size) {
    size_t needed = size - free;
    uint8_t discard[needed];
    xStreamBufferReceive(this->handle_, discard, needed, 0);
  }
  return xStreamBufferSend(this->handle_, data, size, 0);
}

size_t RingBuffer::available() const { return xStreamBufferBytesAvailable(this->handle_); }

size_t RingBuffer::free() const { return xStreamBufferSpacesAvailable(this->handle_); }

BaseType_t RingBuffer::reset() { return xStreamBufferReset(this->handle_); }

}  // namespace esphome

#endif
