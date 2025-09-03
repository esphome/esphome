#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <cstdint>

namespace esphome {
namespace resonate {

enum ChunkType : uint8_t {
  CHUNK_TYPE_ENCODED_AUDIO = 0,
  CHUNK_TYPE_DECODED_AUDIO,
  CHUNK_TYPE_PCM_DUMMY_HEADER,
  CHUNK_TYPE_OPUS_DUMMY_HEADER,
  CHUNK_TYPE_FLAC_HEADER,
};

// Audio chunk structure - heap allocated with reference counting
// OWNERSHIP MODEL:
// - Creator allocates chunk with create_audio_chunk() (ref_count = 1)
// - Passing to a queue or storing: call add_ref() before storing
// - Receiving from queue or done using: call release()
// - Last release() deallocates the chunk and its buffer
//
// WHY CUSTOM REFERENCE COUNTING:
// FreeRTOS queues only support copy semantics (xQueueSend/Receive copy data by value).
// Smart pointers like std::shared_ptr cannot be safely copied through FreeRTOS queues
// as they require move semantics or proper copy constructors. By using raw pointers
// with manual reference counting, we achieve zero-copy audio streaming while maintaining
// memory safety across different tasks(threads)/components.
struct AudioChunk {
  uint8_t *buffer{nullptr};            // Pointer to actual audio data
  std::atomic<uint16_t> ref_count{1};  // Reference count (starts at 1)
  size_t allocated_size{0};            // Total allocated size of buffer
  size_t offset{0};                    // Number of bytes to skip in the buffer
  size_t size{0};                      // Number of bytes to read from the buffer after offset
  int64_t server_timestamp{0};         // Server timestamp when this part of the stream was recorded
  ChunkType chunk_type;                // Describes the audio codec header in this packet

  // Add reference to this chunk
  void add_ref() { ref_count.fetch_add(1, std::memory_order_relaxed); }

  // Release reference and deallocate if last reference
  void release();

  // Get pointer to actual audio buffer (with offset applied)
  uint8_t *get_data() const {
    if (buffer != nullptr) {
      return buffer + offset;
    }
    return nullptr;
  }
};

struct DummyHeader {
  uint32_t sample_rate;
  uint8_t bits_per_sample;
  uint8_t channels;
};

// Creates a new heap-allocated chunk with buffer
AudioChunk *create_audio_chunk(size_t data_size);

// Creates a new heap-allocated chunk that takes ownership of existing buffer
// The buffer must have been allocated with RAMAllocator<uint8_t>
AudioChunk *create_audio_chunk_from_buffer(uint8_t *existing_buffer, size_t buffer_size);

bool reallocate_audio_chunk(AudioChunk **chunk, size_t new_size);

class ResonateChunkQueue {
  // Queue of AudioChunk* pointers - simpler memory management with FreeRTOS queues
 public:
  static std::unique_ptr<ResonateChunkQueue> create(size_t length);

  // Clears and deallocates all elements in the queue
  void reset();

  // Receives chunk pointer and stores it in provided pointer
  bool receive_chunk(AudioChunk **chunk, TickType_t ticks_to_wait = 0);

  // Add chunk pointer to back of queue
  // OWNERSHIP: The queue adds its own reference to the chunk
  // Caller retains their reference and must release it when done
  bool add_chunk(AudioChunk *chunk, TickType_t ticks_to_wait);

  // Return number of elements in queue
  uint32_t size() const { return uxQueueMessagesWaiting(this->queue_); }

 protected:
  QueueHandle_t queue_;  // Queue of AudioChunk* pointers
};

}  // namespace resonate
}  // namespace esphome
#endif
