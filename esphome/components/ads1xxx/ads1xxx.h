#pragma once

#include <cstdint>

namespace esphome::ads1xxx {

// Config register fields shared by the ADS101x, ADS111x and ADS1118
static constexpr uint16_t CONFIG_START = 1 << 15;     // reads back as 1 once the conversion is done
static constexpr uint16_t CONFIG_DATA_RATE = 0x00E0;  // all bits set is the fastest rate
static constexpr uint16_t CONFIG_SINGLE_SHOT = 1 << 8;

/// Volts for a conversion result taken with the gain field of `config`; 12-bit results are left-justified.
inline float to_volts(int16_t raw, uint16_t config) {
  const uint8_t gain = (config >> 9) & 0b111;
  // Volts per count: 6.144 V full scale for gain 0, else 8.192 V >> gain
  return raw * (gain ? 0.00025f : 0.0001875f) / (1 << gain);
}

}  // namespace esphome::ads1xxx
