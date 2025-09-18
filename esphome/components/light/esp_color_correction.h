#pragma once

#include "esphome/core/color.h"

namespace esphome {
namespace light {

class ESPColorCorrection {
 public:
  ESPColorCorrection() : max_brightness_(255, 255, 255, 255) {}
  void set_max_brightness(const Color &max_brightness) { this->max_brightness_ = max_brightness; }
  void set_local_brightness(uint8_t local_brightness) { this->local_brightness_ = local_brightness; }
  void calculate_gamma_table(float gamma);
  inline Color color_correct(Color color) const ESPHOME_ALWAYS_INLINE {
    // corrected = (uncorrected * max_brightness * local_brightness) ^ gamma
    return Color(this->color_correct_red(color.red), this->color_correct_green(color.green),
                 this->color_correct_blue(color.blue), this->color_correct_white(color.white));
  }
  inline uint8_t color_correct_red(uint8_t red) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(red, this->max_brightness_.red), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline uint8_t color_correct_green(uint8_t green) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(green, this->max_brightness_.green), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline uint8_t color_correct_blue(uint8_t blue) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(blue, this->max_brightness_.blue), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline uint8_t color_correct_white(uint8_t white) const ESPHOME_ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(white, this->max_brightness_.white), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline Color color_uncorrect(Color color) const ESPHOME_ALWAYS_INLINE {
    // uncorrected = corrected^(1/gamma) / (max_brightness * local_brightness)
    return Color(this->color_uncorrect_red(color.red), this->color_uncorrect_green(color.green),
                 this->color_uncorrect_blue(color.blue), this->color_uncorrect_white(color.white));
  }
  inline uint8_t color_uncorrect_red(uint8_t red) const ESPHOME_ALWAYS_INLINE {
    if (this->max_brightness_.red == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[red] * 255UL;
    uint16_t res = ((uncorrected / this->max_brightness_.red) * 255UL) / this->local_brightness_;
    return (uint8_t) std::min(res, uint16_t(255));
  }
  inline uint8_t color_uncorrect_green(uint8_t green) const ESPHOME_ALWAYS_INLINE {
    if (this->max_brightness_.green == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[green] * 255UL;
    uint16_t res = ((uncorrected / this->max_brightness_.green) * 255UL) / this->local_brightness_;
    return (uint8_t) std::min(res, uint16_t(255));
  }
  inline uint8_t color_uncorrect_blue(uint8_t blue) const ESPHOME_ALWAYS_INLINE {
    if (this->max_brightness_.blue == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[blue] * 255UL;
    uint16_t res = ((uncorrected / this->max_brightness_.blue) * 255UL) / this->local_brightness_;
    return (uint8_t) std::min(res, uint16_t(255));
  }
  inline uint8_t color_uncorrect_white(uint8_t white) const ESPHOME_ALWAYS_INLINE {
    if (this->max_brightness_.white == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[white] * 255UL;
    uint16_t res = ((uncorrected / this->max_brightness_.white) * 255UL) / this->local_brightness_;
    return (uint8_t) std::min(res, uint16_t(255));
  }

 protected:
  uint8_t gamma_table_[256];
  uint8_t gamma_reverse_table_[256];
  Color max_brightness_;
  uint8_t local_brightness_{255};
};

}  // namespace light
}  // namespace esphome
