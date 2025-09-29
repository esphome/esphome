#include "audio_chunk.h"

#if defined(USE_ESP_IDF) && defined(USE_AUDIO)

namespace esphome {
namespace audio {

std::shared_ptr<AudioChunk> create_audio_chunk(size_t data_size) {
  // Create the chunk using std::make_shared (on regular heap)
  auto chunk = std::make_shared<AudioChunk>();

  // Allocate the buffer using RAMAllocator (can use PSRAM)
  auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  chunk->buffer = buffer_allocator.allocate(data_size);
  if (chunk->buffer == nullptr) {
    return nullptr;  // Failed to allocate buffer
  }

  // Initialize chunk fields
  chunk->allocated_size = data_size;
  chunk->size = data_size;  // Default to full buffer
  chunk->offset = 0;

  return chunk;
}

std::shared_ptr<AudioChunk> create_audio_chunk_from_buffer(uint8_t *existing_buffer, size_t buffer_size) {
  // Create the chunk using std::make_shared (on regular heap)
  auto chunk = std::make_shared<AudioChunk>();

  // Take ownership of the existing buffer
  chunk->buffer = existing_buffer;
  chunk->allocated_size = buffer_size;
  chunk->size = buffer_size;  // Default to full buffer
  chunk->offset = 0;

  return chunk;
}

bool shrink_audio_chunk_buffer(const std::shared_ptr<AudioChunk> &chunk) {
  if (!chunk) {
    return false;
  }

  // Only allow shrinking if we're the sole owner
  if (chunk.use_count() != 1) {
    return false;  // Can't safely reallocate if others have references
  }

  // Calculate the actual used size (offset + data)
  size_t needed_size = chunk->offset + chunk->size;

  // Only shrink if it would actually save memory
  if (needed_size >= chunk->allocated_size) {
    return false;  // No memory savings possible
  }

  // Now we can safely shrink since we're the only reference
  auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
  uint8_t *new_buffer = buffer_allocator.reallocate(chunk->buffer, needed_size);
  if (new_buffer != nullptr) {
    // Reallocation succeeded
    chunk->buffer = new_buffer;
    chunk->allocated_size = needed_size;
    // Note: offset and size remain unchanged as they're still valid
    return true;
  }
  return false;
}

}  // namespace audio
}  // namespace esphome

#endif
