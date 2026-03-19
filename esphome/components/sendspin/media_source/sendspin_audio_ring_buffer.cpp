#include "sendspin_audio_ring_buffer.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.ring_buffer";

std::unique_ptr<SendspinAudioRingBuffer> SendspinAudioRingBuffer::create(size_t buffer_size) {
  auto rb = std::unique_ptr<SendspinAudioRingBuffer>(new SendspinAudioRingBuffer());

  rb->size_ = buffer_size;

  RAMAllocator<uint8_t> allocator;
  rb->storage_ = allocator.allocate(rb->size_);
  if (rb->storage_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes for ring buffer", buffer_size);
    return nullptr;
  }

  rb->ring_buffer_ = xRingbufferCreateStatic(rb->size_, RINGBUF_TYPE_NOSPLIT, rb->storage_, &rb->structure_);
  if (rb->ring_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create ring buffer");
    allocator.deallocate(rb->storage_, rb->size_);
    rb->storage_ = nullptr;
    return nullptr;
  }

  return rb;
}

SendspinAudioRingBuffer::~SendspinAudioRingBuffer() {
  if (this->ring_buffer_ != nullptr) {
    vRingbufferDelete(this->ring_buffer_);
    this->ring_buffer_ = nullptr;

    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->storage_, this->size_);
    this->storage_ = nullptr;
  }
}

bool SendspinAudioRingBuffer::write_chunk(const uint8_t *data, size_t data_size, int64_t timestamp,
                                          ChunkType chunk_type, TickType_t ticks_to_wait) {
  size_t total_size = sizeof(AudioRingBufferEntry) + data_size;

  void *acquired_memory = nullptr;
  BaseType_t result = xRingbufferSendAcquire(this->ring_buffer_, &acquired_memory, total_size, ticks_to_wait);
  if (result != pdTRUE || acquired_memory == nullptr) {
    return false;
  }

  // Fill the header
  auto *entry = static_cast<AudioRingBufferEntry *>(acquired_memory);
  entry->timestamp = timestamp;
  entry->chunk_type = chunk_type;
  entry->data_size = data_size;

  // Copy audio data after the header
  if (data_size > 0 && data != nullptr) {
    std::memcpy(entry->data(), data, data_size);
  }

  // Commit the entry
  result = xRingbufferSendComplete(this->ring_buffer_, acquired_memory);
  return result == pdTRUE;
}

AudioRingBufferEntry *SendspinAudioRingBuffer::receive_chunk(TickType_t ticks_to_wait) {
  size_t item_size = 0;
  void *received_item = xRingbufferReceive(this->ring_buffer_, &item_size, ticks_to_wait);
  if (received_item == nullptr) {
    return nullptr;
  }

  return static_cast<AudioRingBufferEntry *>(received_item);
}

void SendspinAudioRingBuffer::return_chunk(AudioRingBufferEntry *entry) {
  if (entry == nullptr) {
    return;
  }
  vRingbufferReturnItem(this->ring_buffer_, entry);
}

void SendspinAudioRingBuffer::reset() {
  size_t item_size = 0;
  void *item = nullptr;
  while ((item = xRingbufferReceive(this->ring_buffer_, &item_size, 0)) != nullptr) {
    vRingbufferReturnItem(this->ring_buffer_, item);
  }
}

}  // namespace sendspin
}  // namespace esphome

#endif
