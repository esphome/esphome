#pragma once

#include "esphome/core/color.h"
#include "esp_hsv_color.h"
#include "esp_color_correction.h"

namespace esphome {
namespace light {

class ESPColorSettable {
 public:
  virtual void set(const Color &color) = 0;
  virtual void set_red(uint8_t red) = 0;
  virtual void set_green(uint8_t green) = 0;
  virtual void set_blue(uint8_t blue) = 0;
  virtual void set_white(uint8_t white) = 0;
  virtual void set_effect_data(uint8_t effect_data) = 0;
  virtual void fade_to_white(uint8_t amnt) = 0;
  virtual void fade_to_black(uint8_t amnt) = 0;
  virtual void lighten(uint8_t delta) = 0;
  virtual void darken(uint8_t delta) = 0;
  void set(const ESPHSVColor &color) { this->set_hsv(color); }
  void set_hsv(const ESPHSVColor &color) {
    Color rgb = color.to_rgb();
    this->set_rgb(rgb.r, rgb.g, rgb.b);
  }
  void set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    this->set_red(red);
    this->set_green(green);
    this->set_blue(blue);
  }
  void set_rgbw(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    this->set_rgb(red, green, blue);
    this->set_white(white);
  }
};

class ESPColorView : public ESPColorSettable {
 public:
  ESPColorView(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *white, uint8_t *effect_data,
               const ESPColorCorrection *color_correction)
      : red_(red),
        green_(green),
        blue_(blue),
        white_(white),
        effect_data_(effect_data),
        color_correction_(color_correction) {}
  ESPColorView &operator=(const Color &rhs) {
    this->set(rhs);
    return *this;
  }
  ESPColorView &operator=(const ESPHSVColor &rhs) {
    this->set_hsv(rhs);
    return *this;
  }
  void set(const Color &color) override { this->set_rgbw(color.r, color.g, color.b, color.w); }
  void set_red(uint8_t red) override { *this->red_ = this->color_correction_->color_correct_red(red); }
  void set_green(uint8_t green) override { *this->green_ = this->color_correction_->color_correct_green(green); }
  void set_blue(uint8_t blue) override { *this->blue_ = this->color_correction_->color_correct_blue(blue); }
  void set_white(uint8_t white) override {
    if (this->white_ == nullptr)
      return;
    *this->white_ = this->color_correction_->color_correct_white(white);
  }
  void set_effect_data(uint8_t effect_data) override {
    if (this->effect_data_ == nullptr)
      return;
    *this->effect_data_ = effect_data;
  }
  void fade_to_white(uint8_t amnt) override { this->set(this->get().fade_to_white(amnt)); }
  void fade_to_black(uint8_t amnt) override { this->set(this->get().fade_to_black(amnt)); }
  void lighten(uint8_t delta) override { this->set(this->get().lighten(delta)); }
  void darken(uint8_t delta) override { this->set(this->get().darken(delta)); }
  Color get() const { return Color(this->get_red(), this->get_green(), this->get_blue(), this->get_white()); }
  uint8_t get_red() const { return this->color_correction_->color_uncorrect_red(*this->red_); }
  uint8_t get_red_raw() const { return *this->red_; }
  uint8_t get_green() const { return this->color_correction_->color_uncorrect_green(*this->green_); }
  uint8_t get_green_raw() const { return *this->green_; }
  uint8_t get_blue() const { return this->color_correction_->color_uncorrect_blue(*this->blue_); }
  uint8_t get_blue_raw() const { return *this->blue_; }
  uint8_t get_white() const {
    if (this->white_ == nullptr)
      return 0;
    return this->color_correction_->color_uncorrect_white(*this->white_);
  }
  uint8_t get_white_raw() const {
    if (this->white_ == nullptr)
      return 0;
    return *this->white_;
  }
  uint8_t get_effect_data() const {
    if (this->effect_data_ == nullptr)
      return 0;
    return *this->effect_data_;
  }
  void raw_set_color_correction(const ESPColorCorrection *color_correction) {
    this->color_correction_ = color_correction;
  }

 protected:
  uint8_t *const red_;
  uint8_t *const green_;
  uint8_t *const blue_;
  uint8_t *const white_;
  uint8_t *const effect_data_;
  const ESPColorCorrection *color_correction_;
};

}  // namespace light
}  // namespace esphome
