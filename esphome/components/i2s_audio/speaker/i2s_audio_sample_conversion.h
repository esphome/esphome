#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::i2s_audio {

static constexpr uint8_t I2S_SLOT_BITS_AUTO = 0;

constexpr uint8_t resolve_i2s_output_bits(uint8_t input_bits, uint8_t slot_bits, bool expand_to_slot_width) {
  if (slot_bits == I2S_SLOT_BITS_AUTO) {
    return input_bits;
  }
  if (input_bits > slot_bits || (expand_to_slot_width && input_bits < slot_bits)) {
    return slot_bits;
  }
  return input_bits;
}

struct I2SAudioExpansion {
  uint8_t *output_data;
  size_t input_bytes;
  size_t output_bytes;
  uint32_t frames;
};

constexpr I2SAudioExpansion prepare_i2s_audio_expansion(uint8_t *scratch, uint8_t input_bytes_per_sample,
                                                        uint8_t output_bytes_per_sample, uint8_t channels,
                                                        uint32_t frames) {
  const size_t samples = static_cast<size_t>(frames) * channels;
  return {
      .output_data = scratch,
      .input_bytes = samples * input_bytes_per_sample,
      .output_bytes = samples * output_bytes_per_sample,
      .frames = frames,
  };
}

}  // namespace esphome::i2s_audio
