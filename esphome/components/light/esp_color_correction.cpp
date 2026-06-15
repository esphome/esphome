#include "esp_color_correction.h"

namespace esphome::light {

uint8_t ESPColorCorrection::gamma_correct_(uint8_t value) const {
  if (this->gamma_table_ == nullptr)
    return value;
  return static_cast<uint8_t>((progmem_read_uint16(&this->gamma_table_[value]) + 128) / 257);
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

Color ESPColorCorrection::color_uncorrect(Color color) const {
  // uncorrected = corrected^(1/gamma) / (max_brightness * local_brightness)
  return Color(this->color_uncorrect_red(color.red), this->color_uncorrect_green(color.green),
               this->color_uncorrect_blue(color.blue), this->color_uncorrect_white(color.white));
}

uint8_t ESPColorCorrection::color_uncorrect_channel_(uint8_t value, uint8_t max_brightness) const {
  if (max_brightness == 0 || this->local_brightness_ == 0)
    return 0;
  // Use 32-bit intermediates: when max_brightness and local_brightness_ are small but non-zero,
  // (uncorrected / max_brightness) * 255 can exceed 65535 before the std::min(255) clamp runs.
  uint32_t uncorrected = this->gamma_uncorrect_(value) * 255UL;
  uint32_t res = ((uncorrected / max_brightness) * 255UL) / this->local_brightness_;
  return static_cast<uint8_t>(std::min(res, uint32_t(255)));
}

}  // namespace esphome::light
