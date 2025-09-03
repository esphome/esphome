#include "resonate_chunk_queue.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

namespace esphome {
namespace resonate {

AudioChunk *create_audio_chunk(size_t data_size) {
  // Allocate the chunk itself
  auto chunk_allocator = RAMAllocator<AudioChunk>(RAMAllocator<AudioChunk>::NONE);
  AudioChunk *chunk = chunk_allocator.allocate(1);
  if (chunk == nullptr) {
    return nullptr;
  }

  // Initialize chunk with defaults
  new (chunk) AudioChunk();  // Placement new to initialize

  // Allocate the buffer
  auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  chunk->buffer = buffer_allocator.allocate(data_size);
  if (chunk->buffer == nullptr) {
    chunk_allocator.deallocate(chunk, 1);
    return nullptr;
  }

  chunk->ref_count = 1;  // Start with one reference
  chunk->allocated_size = data_size;
  chunk->size = data_size;  // Default to full buffer
  chunk->offset = 0;

  return chunk;
}

AudioChunk *create_audio_chunk_from_buffer(uint8_t *existing_buffer, size_t buffer_size) {
  // Allocate the chunk itself
  auto chunk_allocator = RAMAllocator<AudioChunk>(RAMAllocator<AudioChunk>::NONE);
  AudioChunk *chunk = chunk_allocator.allocate(1);
  if (chunk == nullptr) {
    return nullptr;
  }

  // Initialize chunk with defaults
  new (chunk) AudioChunk();  // Placement new to initialize

  // Take ownership of the existing buffer
  chunk->buffer = existing_buffer;
  chunk->ref_count = 1;  // Start with one reference
  chunk->allocated_size = buffer_size;
  chunk->size = buffer_size;  // Default to full buffer
  chunk->offset = 0;

  return chunk;
}

bool reallocate_audio_chunk(AudioChunk **chunk, size_t new_size) {
  // Attempt to reallocate the buffer
  auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  uint8_t *new_buffer = buffer_allocator.reallocate((*chunk)->buffer, new_size);
  if (new_buffer != nullptr) {
    // Reallocation succeeded
    (*chunk)->buffer = new_buffer;
    (*chunk)->allocated_size = new_size;
    return true;
  }
  return false;
}

void AudioChunk::release() {
  // Decrement reference count atomically and check if we should deallocate
  // Using memory_order_acq_rel to ensure proper synchronization
  if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    // We were definitely the last reference, safe to deallocate
    if (buffer != nullptr) {
      auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
      buffer_allocator.deallocate(buffer, allocated_size);
    }

    // Deallocate the chunk itself
    auto chunk_allocator = RAMAllocator<AudioChunk>(RAMAllocator<AudioChunk>::NONE);
    chunk_allocator.deallocate(this, 1);
  }
}

std::unique_ptr<ResonateChunkQueue> ResonateChunkQueue::create(size_t length) {
  std::unique_ptr<ResonateChunkQueue> chunk_queue = make_unique<ResonateChunkQueue>();
  // Queue now holds AudioChunk* pointers
  chunk_queue->queue_ = xQueueCreate(length, sizeof(AudioChunk *));
  if (chunk_queue->queue_ == nullptr) {
    return nullptr;
  }
  return chunk_queue;
}

void ResonateChunkQueue::reset() {
  // Receive and release all chunks in the queue
  AudioChunk *chunk;
  while (this->receive_chunk(&chunk, 0)) {
    chunk->release();
  }
}

bool ResonateChunkQueue::receive_chunk(AudioChunk **chunk, TickType_t ticks_to_wait) {
  return xQueueReceive(this->queue_, chunk, ticks_to_wait);
}

bool ResonateChunkQueue::add_chunk(AudioChunk *chunk, TickType_t ticks_to_wait) {
  if (chunk == nullptr) {
    return false;
  }

  // Queue takes its own reference
  chunk->add_ref();

  if (xQueueSend(this->queue_, &chunk, ticks_to_wait) == pdTRUE) {
    return true;
  }

  // Failed to add to queue, release the reference we just added
  chunk->release();
  return false;
}

}  // namespace resonate
}  // namespace esphome

#endif
