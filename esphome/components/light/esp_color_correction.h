#pragma once

#include "esphome/core/color.h"
#include "esphome/core/hal.h"

namespace esphome::light {

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
  void set_gamma_table(const uint16_t *table) { this->gamma_table_ = table; }
  inline Color color_correct(Color color) const ESPHOME_ALWAYS_INLINE {
    // corrected = (uncorrected * max_brightness * local_brightness) ^ gamma
    return Color(this->color_correct_red(color.red), this->color_correct_green(color.green),
                 this->color_correct_blue(color.blue), this->color_correct_white(color.white));
  }
  inline uint8_t color_correct_red(uint8_t red) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8_twice(red, this->max_brightness_.red, this->local_brightness_);
    return this->gamma_correct_(res);
  }
  inline uint8_t color_correct_green(uint8_t green) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8_twice(green, this->max_brightness_.green, this->local_brightness_);
    return this->gamma_correct_(res);
  }
  inline uint8_t color_correct_blue(uint8_t blue) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8_twice(blue, this->max_brightness_.blue, this->local_brightness_);
    return this->gamma_correct_(res);
  }
  inline uint8_t color_correct_white(uint8_t white) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8_twice(white, this->max_brightness_.white, this->local_brightness_);
    return this->gamma_correct_(res);
  }
  Color color_uncorrect(Color color) const;
  inline uint8_t color_uncorrect_red(uint8_t red) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel_(red, this->max_brightness_.red);
  }
  inline uint8_t color_uncorrect_green(uint8_t green) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel_(green, this->max_brightness_.green);
  }
  inline uint8_t color_uncorrect_blue(uint8_t blue) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel_(blue, this->max_brightness_.blue);
  }
  inline uint8_t color_uncorrect_white(uint8_t white) const ESPHOME_ALWAYS_INLINE {
    return this->color_uncorrect_channel_(white, this->max_brightness_.white);
  }

 protected:
  /// Forward gamma: read uint16 PROGMEM table, convert to uint8
  uint8_t gamma_correct_(uint8_t value) const;
  /// Reverse gamma: binary search the forward PROGMEM table
  uint8_t gamma_uncorrect_(uint8_t value) const;
  /// Shared body of color_uncorrect_{red,green,blue,white}. Kept out-of-line
  /// to avoid duplicating two 16-bit divides at every call site.
  uint8_t color_uncorrect_channel_(uint8_t value, uint8_t max_brightness) const;

  const uint16_t *gamma_table_{nullptr};
  Color max_brightness_{255, 255, 255, 255};
  uint8_t local_brightness_{255};
};

}  // namespace esphome::light
