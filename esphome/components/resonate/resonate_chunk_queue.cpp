#include "resonate_chunk_queue.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

namespace esphome {
namespace resonate {

bool allocate_audio_chunk(size_t data_size, AudioChunk *chunk) {
  auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  chunk->data = allocator.allocate(data_size);
  return chunk->data != nullptr;
}

void deallocate_audio_chunk(AudioChunk *chunk) {
  auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  allocator.deallocate(chunk->data, chunk->offset + chunk->size);
  chunk->data = nullptr;
}

std::unique_ptr<ResonateChunkQueue> ResonateChunkQueue::create(size_t length) {
  std::unique_ptr<ResonateChunkQueue> chunk_queue = make_unique<ResonateChunkQueue>();
  chunk_queue->queue_ = xQueueCreate(length, sizeof(AudioChunk));
  if (chunk_queue->queue_ == nullptr) {
    return nullptr;
  }
  return chunk_queue;
}

void ResonateChunkQueue::reset() {
  while (this->receive_chunk(true)) {
  }
}

bool ResonateChunkQueue::peek_chunk(AudioChunk *chunk, TickType_t ticks_to_wait) {
  return xQueuePeek(this->queue_, chunk, ticks_to_wait);
}

bool ResonateChunkQueue::receive_chunk(bool deallocate) {
  AudioChunk chunk;
  if (xQueueReceive(this->queue_, &chunk, 0)) {
    if (deallocate) {
      deallocate_audio_chunk(&chunk);
    }
    return true;
  }
  return false;
}

bool ResonateChunkQueue::add_chunk(const AudioChunk *chunk, TickType_t ticks_to_wait) {
  return xQueueSend(this->queue_, chunk, ticks_to_wait);
}

}  // namespace resonate
}  // namespace esphome

#endif
