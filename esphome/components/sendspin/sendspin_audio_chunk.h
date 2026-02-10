#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_PLAYER)

#include <cstdint>

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

}  // namespace sendspin
}  // namespace esphome
#endif
