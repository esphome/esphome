#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/audio/audio_chunk.h"

#include <memory>

namespace esphome {
namespace sendspin {

enum ChunkType : uint8_t {
  CHUNK_TYPE_ENCODED_AUDIO = 0,
  CHUNK_TYPE_DECODED_AUDIO,
  CHUNK_TYPE_PCM_DUMMY_HEADER,
  CHUNK_TYPE_OPUS_DUMMY_HEADER,
  CHUNK_TYPE_FLAC_HEADER,
};

struct DummyHeader {
  uint32_t sample_rate;
  uint8_t bits_per_sample;
  uint8_t channels;
};

// Sendspin-specific audio chunk with additional metadata
struct SendspinAudioChunk : public audio::AudioChunk {
  int64_t timestamp{0};  // Timestamp when this part of the stream was recorded
  ChunkType chunk_type;  // Describes the audio codec header in this packet
  // Add any other sendspin-specific fields here in the future
};

// Factory functions for creating SendspinAudioChunks (for use with AudioChunkQueue)
std::shared_ptr<SendspinAudioChunk> create_sendspin_chunk(size_t data_size);
std::shared_ptr<SendspinAudioChunk> create_sendspin_chunk_from_buffer(uint8_t *existing_buffer, size_t buffer_size);

}  // namespace sendspin
}  // namespace esphome
#endif
