#pragma once

#include <algorithm>
#include <cmath>

#include "esphome/components/light/esp_color_correction.h"

namespace esphome::light::testing {

/// A gamma table as codegen builds it (generate_gamma_table() in light/__init__.py): non-zero indices are clamped to
/// at least 1, so a low level is not quantised to 0.
inline const GammaTable &production_gamma_table() {
  static const GammaTable TABLE = [] {
    GammaTable table{};
    for (int i = 1; i < 256; i++)
      table.lut[i] =
          static_cast<uint16_t>(std::max(1L, std::min(65535L, std::lround(std::pow(i / 255.0, 2.8) * 65535.0))));
    table.gamma_x100 = 280;
    return table;
  }();
  return TABLE;
}

}  // namespace esphome::light::testing
