#include "esp_color_correction.h"

namespace esphome::light {

uint8_t ESPColorCorrection::gamma_correct_(uint8_t value) const {
  if (this->gamma_table_ == nullptr)
    return value;
  uint16_t table_value = progmem_read_uint16(&this->gamma_table_[value]);
  uint8_t result = (table_value + 128) / 257;
  if (result == 0 && table_value != 0)
    return 1;
  return result;
}

uint16_t ESPColorCorrection::gamma_level_(uint8_t value) const {
  if (this->gamma_table_ == nullptr)
    return value * 257;
  return progmem_read_uint16(&this->gamma_table_[value]);
}

uint8_t ESPColorCorrection::gamma_round_(uint8_t value) const {
  if (this->gamma_table_ == nullptr)
    return value;
  return (progmem_read_uint16(&this->gamma_table_[value]) + 128) / 257;
}

// Lighting every non-zero channel instead would turn a dim red with a trace of green into yellow
Color ESPColorCorrection::correct_dark_pixel_(Color color) const {
  // Compared without the local brightness, which is the same for every channel: the scaled 8-bit
  // values are too coarse to compare at the lowest brightness
  uint16_t strengths[4];
  uint16_t max_strength = 0;
  bool lit = false;
  for (uint8_t i = 0; i < 4; i++) {
    strengths[i] = color[i] * this->max_brightness_[i];
    max_strength = std::max(max_strength, strengths[i]);
    lit |= esp_scale8_twice(color[i], this->max_brightness_[i], this->local_brightness_) != 0;
  }
  Color res;
  if (!lit)
    return res;
  // Stretch the strongest channel to full scale and round the others against it; the table is too
  // coarse to compare its lowest levels
  for (uint8_t i = 0; i < 4; i++) {
    if (strengths[i] == max_strength) {
      res[i] = 1;
    } else if (strengths[i] != 0) {
      res[i] = this->gamma_level_(strengths[i] * 255 / max_strength) >= 0x8000 ? 1 : 0;
    }
  }
  return res;
}

uint8_t ESPColorCorrection::gamma_uncorrect_(uint8_t value) const {
  if (this->gamma_table_ == nullptr)
    return value;
  if (value == 0)
    return 0;
  uint16_t target = value * 257;  // Scale 0-255 to 0-65535
  uint8_t lo = gamma_table_reverse_search(this->gamma_table_, target);
  if (lo >= 255)
    return 255;
  uint16_t a = progmem_read_uint16(&this->gamma_table_[lo]);
  uint16_t b = progmem_read_uint16(&this->gamma_table_[lo + 1]);
  return (target - a <= b - target) ? lo : lo + 1;
}

// Under the limit, gamma and the limit are undone together: the largest level that encodes to at most the
// stored one, so a read, change and write of a pixel re-encodes to what was stored
uint8_t ESPColorCorrection::uncorrect_level_(uint8_t value) const {
#ifdef USE_LIGHT_POWER_ESTIMATION
  if (this->power_brightness_ == 255)
    return this->gamma_uncorrect_(value);
  uint8_t lo = 0;
  uint8_t hi = 255;
  while (lo < hi) {
    const uint8_t mid = (lo + hi + 1) / 2;
    if (this->limit_(this->gamma_correct_(mid)) <= value) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return lo;
#else
  return this->gamma_uncorrect_(value);
#endif
}

uint8_t ESPColorCorrection::color_uncorrect_channel(uint8_t channel, uint8_t raw_value) const {
  const uint8_t max_brightness = this->max_brightness_[channel];
  if (max_brightness == 0 || this->local_brightness_ == 0)
    return 0;
  // Use 32-bit intermediates: when max_brightness and local_brightness_ are small but non-zero,
  // (uncorrected / max_brightness) * 255 can exceed 65535 before the std::min(255) clamp runs.
  uint32_t uncorrected = this->uncorrect_level_(raw_value) * 255UL;
  uint32_t res = ((uncorrected / max_brightness) * 255UL) / this->local_brightness_;
  return static_cast<uint8_t>(std::min(res, uint32_t(255)));
}

}  // namespace esphome::light
