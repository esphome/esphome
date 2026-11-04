#pragma once

#include "esphome/core/color.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include <algorithm>

namespace esphome::light {

/// A gamma curve as codegen emits it into PROGMEM: the lookup table and the gamma it was built from
struct GammaTable {
  uint16_t lut[256];
  uint16_t gamma_x100;
};

/// Binary search a monotonically increasing uint16[256] PROGMEM table.
/// Returns the largest index where table[index] <= target.
inline uint8_t gamma_table_reverse_search(const uint16_t *table, uint16_t target) {
  uint8_t lo = 0, hi = 255;
  while (lo < hi) {
    uint8_t mid = (lo + hi + 1) / 2;
    if (progmem_read_uint16(&table[mid]) <= target) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return lo;
}

class ESPColorCorrection {
 public:
  void set_max_brightness(const Color &max_brightness) { this->max_brightness_ = max_brightness; }
  void set_local_brightness(uint8_t local_brightness) { this->local_brightness_ = local_brightness; }
#ifdef USE_LIGHT_POWER_ESTIMATION
  // The current limit's factor, applied after gamma
  void set_power_brightness(uint8_t power_brightness) { this->power_brightness_ = power_brightness; }
  uint8_t get_power_brightness() const { return this->power_brightness_; }
#endif
  void set_gamma_table(const uint16_t *table) { this->gamma_table_ = table; }

  inline Color color_correct(Color color) const ESPHOME_ALWAYS_INLINE {
    // corrected = (uncorrected * max_brightness * local_brightness) ^ gamma
    return Color(this->color_correct_red(color.red), this->color_correct_green(color.green),
                 this->color_correct_blue(color.blue), this->color_correct_white(color.white));
  }
  inline uint8_t color_correct_red(uint8_t red) const ESPHOME_ALWAYS_INLINE {
    return this->color_correct_channel(COLOR_CHANNEL_R, red);
  }
  inline uint8_t color_correct_green(uint8_t green) const ESPHOME_ALWAYS_INLINE {
    return this->color_correct_channel(COLOR_CHANNEL_G, green);
  }
  inline uint8_t color_correct_blue(uint8_t blue) const ESPHOME_ALWAYS_INLINE {
    return this->color_correct_channel(COLOR_CHANNEL_B, blue);
  }
  inline uint8_t color_correct_white(uint8_t white) const ESPHOME_ALWAYS_INLINE {
    return this->color_correct_channel(COLOR_CHANNEL_W, white);
  }
  inline uint8_t color_correct_channel(uint8_t channel, uint8_t value) const ESPHOME_ALWAYS_INLINE {
    return this->limit_(
        this->gamma_correct_(esp_scale8_twice(value, this->max_brightness_[channel], this->local_brightness_)));
  }

  inline Color color_uncorrect(Color raw_color) const {
    // uncorrected = corrected^(1/gamma) / (max_brightness * local_brightness)
    return Color(this->color_uncorrect_red(raw_color.red), this->color_uncorrect_green(raw_color.green),
                 this->color_uncorrect_blue(raw_color.blue), this->color_uncorrect_white(raw_color.white));
  }
  inline uint8_t color_uncorrect_red(uint8_t raw_red) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel(COLOR_CHANNEL_R, raw_red);
  }
  inline uint8_t color_uncorrect_green(uint8_t raw_green) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel(COLOR_CHANNEL_G, raw_green);
  }
  inline uint8_t color_uncorrect_blue(uint8_t raw_blue) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel(COLOR_CHANNEL_B, raw_blue);
  }
  inline uint8_t color_uncorrect_white(uint8_t raw_white) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel(COLOR_CHANNEL_W, raw_white);
  }
  uint8_t color_uncorrect_channel(uint8_t channel, uint8_t raw_value) const;

 protected:
  // The limit on a gamma-corrected level; rounded up, so a lit channel stays lit
  inline uint8_t limit_(uint8_t res) const ESPHOME_ALWAYS_INLINE {
#ifdef USE_LIGHT_POWER_ESTIMATION
    if (res != 0)
      res = std::max<uint8_t>(esp_scale8(res, this->power_brightness_), 1);
#endif
    return res;
  }
  /// Forward gamma: read uint16 PROGMEM table, convert to uint8
  uint8_t gamma_correct_(uint8_t value) const;
  /// Reverse gamma and limit for one stored level
  uint8_t uncorrect_level_(uint8_t value) const;
  /// Reverse gamma: binary search the forward PROGMEM table
  uint8_t gamma_uncorrect_(uint8_t value) const;

  const uint16_t *gamma_table_{nullptr};
  Color max_brightness_{255, 255, 255, 255};
  uint8_t local_brightness_{255};
#ifdef USE_LIGHT_POWER_ESTIMATION
  uint8_t power_brightness_{255};
#endif
};

}  // namespace esphome::light
