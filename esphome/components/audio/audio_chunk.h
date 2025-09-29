#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_AUDIO)

#include "esphome/core/helpers.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace esphome {
namespace audio {

// Base audio chunk structure for zero-copy audio streaming
// Used with std::shared_ptr for automatic memory management
struct AudioChunk {
  uint8_t *buffer{nullptr};  // Pointer to actual audio data (may be in PSRAM)
  size_t allocated_size{0};  // Total allocated size of buffer
  size_t offset{0};          // Number of bytes to skip in the buffer
  size_t size{0};            // Number of bytes to read from the buffer after offset

  // Get pointer to actual audio buffer (with offset applied)
  // Returns nullptr if buffer is null or offset is out of bounds
  uint8_t *get_data() const {
    if (buffer != nullptr && offset <= allocated_size) {
      return buffer + offset;
    }
    return nullptr;
  }

  // Get the actual usable size of data (respecting offset and size)
  size_t get_usable_size() const {
    if (offset > allocated_size) {
      return 0;
    }
    // Ensure size doesn't exceed what's actually available after offset
    size_t available = allocated_size - offset;
    return (size <= available) ? size : available;
  }

  // Virtual destructor for proper cleanup of derived classes
  // Deallocates the buffer when the shared_ptr is destroyed
  virtual ~AudioChunk() {
    if (buffer != nullptr) {
      auto buffer_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
      buffer_allocator.deallocate(buffer, allocated_size);
      buffer = nullptr;
    }
  }
};

// Factory functions for creating AudioChunks
// These allocate the AudioChunk struct with std::make_shared but the buffer with RAMAllocator
std::shared_ptr<AudioChunk> create_audio_chunk(size_t data_size);
std::shared_ptr<AudioChunk> create_audio_chunk_from_buffer(uint8_t *existing_buffer, size_t buffer_size);

// Shrinks the buffer of an AudioChunk to its actually used size (offset + size)
// Only works if use_count() == 1 (we're the sole owner) and would actually save memory
// Returns true if successfully shrunk, false otherwise (original buffer remains unchanged)
bool shrink_audio_chunk_buffer(const std::shared_ptr<AudioChunk> &chunk);

}  // namespace audio
}  // namespace esphome
#endif
