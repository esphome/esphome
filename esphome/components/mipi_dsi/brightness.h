#pragma once

#include <cmath>
#include <cstdint>

namespace esphome::mipi_dsi {

// A light brightness from 0 to 1, scaled into [min_brightness, max_brightness]
inline uint8_t scale_brightness(float brightness, uint8_t min_brightness, uint8_t max_brightness) {
  return min_brightness + static_cast<uint8_t>(roundf(brightness * (max_brightness - min_brightness)));
}

}  // namespace esphome::mipi_dsi
