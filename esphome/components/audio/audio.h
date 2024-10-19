#pragma once

#include <cstdint>
#include <stddef.h>

namespace esphome {
namespace audio {

struct AudioStreamInfo {
  bool operator==(const AudioStreamInfo &rhs) const {
    return (channels == rhs.channels) && (bits_per_sample == rhs.bits_per_sample) && (sample_rate == rhs.sample_rate);
  }
  bool operator!=(const AudioStreamInfo &rhs) const { return !operator==(rhs); }
  size_t get_bytes_per_sample() const { return bits_per_sample / 8; }
  uint8_t channels = 1;
  uint8_t bits_per_sample = 16;
  uint32_t sample_rate = 16000;
};

}  // namespace audio
}  // namespace esphome
