#pragma once

#include <cstdint>

namespace esphome::ads1xxx {

// Config register fields shared by the ADS101x, ADS111x and ADS1118
static constexpr uint16_t CONFIG_START = 1 << 15;  // reads back as 1 once the conversion is done
static constexpr uint16_t CONFIG_MUX_GAIN = 0x7E00;
static constexpr uint16_t CONFIG_DATA_RATE = 0x00E0;
static constexpr uint16_t CONFIG_SINGLE_SHOT = 1 << 8;

/// Volts for a conversion result taken with the gain field of `config`; 12-bit results are left-justified.
inline float to_volts(int16_t raw, uint16_t config) {
  const uint8_t gain = (config >> 9) & 0b111;
  const float full_scale = gain == 0 ? 6.144f : 8.192f / (1 << gain);
  return raw * full_scale / 32768.0f;
}

}  // namespace esphome::ads1xxx
