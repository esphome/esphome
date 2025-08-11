#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

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

// Stores encoded audio chunks sent from the server
// TODO: We have generalized this. Encoded audio using the server timestamps, but once decoded they are in reference to
// client timestamps
struct AudioChunk {
  uint8_t *data{nullptr};    // Pointer to encoded audio data. Must be deallocated after receiving
  size_t offset;             // Number of bytes to skip in the data pointer to skip
  size_t size;               // Number of bytes to read from the data pointer after the offset
  int64_t server_timestamp;  // Server timestamp when this part of the stream was recorded
  uint32_t frame_count;
  ChunkType chunk_type;  // Describes the audio codec header in this packet/its only an encoded chunk
};

struct DummyHeader {
  uint32_t sample_rate;
  uint8_t bits_per_sample;
  uint8_t channels;
};

// Creates a new chunk and allocates memory for the data
bool allocate_audio_chunk(size_t data_size, AudioChunk *chunk);

// Deallocates the data in the chunk
void deallocate_audio_chunk(AudioChunk *chunk);

class ResonateChunkQueue {
  // TODO: Generalize this and make it part of the audio component. Then any other synced audio component can use this
  // approach
 public:
  static std::unique_ptr<ResonateChunkQueue> create(size_t length);

  // Clears and deallocates all elements in the queue
  void reset();

  // Peeks at the chunk at the front of the queue
  bool peek_chunk(AudioChunk *chunk, TickType_t ticks_to_wait);

  // Receives and optionally deallocates the chunk at the frontof the queue
  bool receive_chunk(bool deallocate);

  // Add chunk to back of queue
  bool add_chunk(const AudioChunk *chunk, TickType_t ticks_to_wait);

  // Return number of elements in queue
  uint32_t size() const { return uxQueueMessagesWaiting(this->queue_); }

 protected:
  QueueHandle_t queue_;
};

}  // namespace resonate
}  // namespace esphome
#endif
