#include "resonate_audio_chunk.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

namespace esphome {
namespace resonate {

std::shared_ptr<ResonateAudioChunk> create_resonate_chunk(size_t data_size) {
  // Create the chunk using std::make_shared (on regular heap)
  auto chunk = std::make_shared<ResonateAudioChunk>();

  // Allocate the buffer using RAMAllocator (can use PSRAM)
  auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  chunk->buffer = buffer_allocator.allocate(data_size);
  if (chunk->buffer == nullptr) {
    return nullptr;  // Failed to allocate buffer
  }

  // Initialize base AudioChunk fields
  chunk->allocated_size = data_size;
  chunk->size = data_size;  // Default to full buffer
  chunk->offset = 0;

  // Initialize ResonateAudioChunk-specific fields
  chunk->timestamp = 0;
  chunk->chunk_type = CHUNK_TYPE_ENCODED_AUDIO;

  return chunk;
}

std::shared_ptr<ResonateAudioChunk> create_resonate_chunk_from_buffer(uint8_t *existing_buffer, size_t buffer_size) {
  // Create the chunk using std::make_shared (on regular heap)
  auto chunk = std::make_shared<ResonateAudioChunk>();

  // Take ownership of the existing buffer
  chunk->buffer = existing_buffer;
  chunk->allocated_size = buffer_size;
  chunk->size = buffer_size;  // Default to full buffer
  chunk->offset = 0;

  // Initialize ResonateAudioChunk-specific fields
  chunk->timestamp = 0;
  chunk->chunk_type = CHUNK_TYPE_ENCODED_AUDIO;

  return chunk;
}

}  // namespace resonate
}  // namespace esphome

#endif
