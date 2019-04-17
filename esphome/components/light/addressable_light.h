#pragma once

#include "esphome/core/component.h"
#include "light_output.h"
#include "light_state.h"

namespace esphome {
namespace light {

inline static uint8_t esp_scale8(uint8_t i, uint8_t scale) { return (uint16_t(i) * (1 + uint16_t(scale))) / 256; }

struct ESPColor {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
      union {
        uint8_t w;
        uint8_t white;
      };
    };
    uint8_t raw[4];
  };
  inline ESPColor() ALWAYS_INLINE : r(0), g(0), b(0), w(0) {}  // NOLINT
  inline ESPColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) ALWAYS_INLINE : r(red),
                                                                                           g(green),
                                                                                           b(blue),
                                                                                           w(white) {}
  inline ESPColor(uint8_t red, uint8_t green, uint8_t blue) ALWAYS_INLINE : r(red), g(green), b(blue), w(0) {}
  inline ESPColor(uint32_t colorcode) ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                      g((colorcode >> 8) & 0xFF),
                                                      b((colorcode >> 0) & 0xFF),
                                                      w((colorcode >> 24) & 0xFF) {}
  inline ESPColor(const ESPColor &rhs) ALWAYS_INLINE {
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    this->w = rhs.w;
  }
  inline bool is_on() ALWAYS_INLINE { return this->r != 0 || this->g != 0 || this->b != 0 || this->w != 0; }
  inline ESPColor &operator=(const ESPColor &rhs) ALWAYS_INLINE {
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    this->w = rhs.w;
    return *this;
  }
  inline ESPColor &operator=(uint32_t colorcode) ALWAYS_INLINE {
    this->w = (colorcode >> 24) & 0xFF;
    this->r = (colorcode >> 16) & 0xFF;
    this->g = (colorcode >> 8) & 0xFF;
    this->b = (colorcode >> 0) & 0xFF;
    return *this;
  }
  inline uint8_t &operator[](uint8_t x) ALWAYS_INLINE { return this->raw[x]; }
  inline ESPColor operator*(uint8_t scale) const ALWAYS_INLINE {
    return ESPColor(esp_scale8(this->red, scale), esp_scale8(this->green, scale), esp_scale8(this->blue, scale),
                    esp_scale8(this->white, scale));
  }
  inline ESPColor &operator*=(uint8_t scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale);
    this->green = esp_scale8(this->green, scale);
    this->blue = esp_scale8(this->blue, scale);
    this->white = esp_scale8(this->white, scale);
    return *this;
  }
  inline ESPColor operator*(const ESPColor &scale) const ALWAYS_INLINE {
    return ESPColor(esp_scale8(this->red, scale.red), esp_scale8(this->green, scale.green),
                    esp_scale8(this->blue, scale.blue), esp_scale8(this->white, scale.white));
  }
  inline ESPColor &operator*=(const ESPColor &scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale.red);
    this->green = esp_scale8(this->green, scale.green);
    this->blue = esp_scale8(this->blue, scale.blue);
    this->white = esp_scale8(this->white, scale.white);
    return *this;
  }
  inline ESPColor operator+(const ESPColor &add) const ALWAYS_INLINE {
    ESPColor ret;
    if (uint8_t(add.r + this->r) < this->r)
      ret.r = 255;
    else
      ret.r = this->r + add.r;
    if (uint8_t(add.g + this->g) < this->g)
      ret.g = 255;
    else
      ret.g = this->g + add.g;
    if (uint8_t(add.b + this->b) < this->b)
      ret.b = 255;
    else
      ret.b = this->b + add.b;
    if (uint8_t(add.w + this->w) < this->w)
      ret.w = 255;
    else
      ret.w = this->w + add.w;
    return ret;
  }
  inline ESPColor &operator+=(const ESPColor &add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline ESPColor operator+(uint8_t add) const ALWAYS_INLINE { return (*this) + ESPColor(add, add, add, add); }
  inline ESPColor &operator+=(uint8_t add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline ESPColor operator-(const ESPColor &subtract) const ALWAYS_INLINE {
    ESPColor ret;
    if (subtract.r > this->r)
      ret.r = 0;
    else
      ret.r = this->r - subtract.r;
    if (subtract.g > this->g)
      ret.g = 0;
    else
      ret.g = this->g - subtract.g;
    if (subtract.b > this->b)
      ret.b = 0;
    else
      ret.b = this->b - subtract.b;
    if (subtract.w > this->w)
      ret.w = 0;
    else
      ret.w = this->w - subtract.w;
    return ret;
  }
  inline ESPColor &operator-=(const ESPColor &subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  inline ESPColor operator-(uint8_t subtract) const ALWAYS_INLINE {
    return (*this) - ESPColor(subtract, subtract, subtract, subtract);
  }
  inline ESPColor &operator-=(uint8_t subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  static ESPColor random_color() {
    uint32_t rand = random_uint32();
    uint8_t w = rand >> 24;
    uint8_t r = rand >> 16;
    uint8_t g = rand >> 8;
    uint8_t b = rand >> 0;
    const uint16_t max_rgb = std::max(r, std::max(g, b));
    return ESPColor(uint8_t((uint16_t(r) * 255U / max_rgb)), uint8_t((uint16_t(g) * 255U / max_rgb)),
                    uint8_t((uint16_t(b) * 255U / max_rgb)), w);
  }
};

struct ESPHSVColor {
  union {
    struct {
      union {
        uint8_t hue;
        uint8_t h;
      };
      union {
        uint8_t saturation;
        uint8_t s;
      };
      union {
        uint8_t value;
        uint8_t v;
      };
    };
    uint8_t raw[3];
  };
  inline ESPHSVColor() ALWAYS_INLINE : h(0), s(0), v(0) {  // NOLINT
  }
  inline ESPHSVColor(uint8_t hue, uint8_t saturation, uint8_t value) ALWAYS_INLINE : hue(hue),
                                                                                     saturation(saturation),
                                                                                     value(value) {}
  ESPColor to_rgb() const {
    // based on FastLED's hsv rainbow to rgb
    const uint8_t hue = this->hue;
    const uint8_t sat = this->saturation;
    const uint8_t val = this->value;
    // upper 3 hue bits are for branch selection, lower 5 are for values
    const uint8_t offset8 = (hue & 0x1F) << 3;  // 0..248
    // third of the offset, 255/3 = 85 (actually only up to 82; 164)
    const uint8_t third = esp_scale8(offset8, 85);
    const uint8_t two_thirds = esp_scale8(offset8, 170);
    ESPColor rgb(255, 255, 255, 0);
    switch (hue >> 5) {
      case 0b000:
        rgb.r = 255 - third;
        rgb.g = third;
        rgb.b = 0;
        break;
      case 0b001:
        rgb.r = 171;
        rgb.g = 85 + third;
        rgb.b = 0;
        break;
      case 0b010:
        rgb.r = 171 - two_thirds;
        rgb.g = 170 + third;
        rgb.b = 0;
        break;
      case 0b011:
        rgb.r = 0;
        rgb.g = 255 - third;
        rgb.b = third;
        break;
      case 0b100:
        rgb.r = 0;
        rgb.g = 171 - two_thirds;
        rgb.b = 85 + two_thirds;
        break;
      case 0b101:
        rgb.r = third;
        rgb.g = 0;
        rgb.b = 255 - third;
        break;
      case 0b110:
        rgb.r = 85 + third;
        rgb.g = 0;
        rgb.b = 171 - third;
        break;
      case 0b111:
        rgb.r = 170 + third;
        rgb.g = 0;
        rgb.b = 85 - third;
        break;
      default:
        break;
    }
    // low saturation -> add uniform color to orig. hue
    // high saturation -> use hue directly
    // scales with square of saturation
    // (r,g,b) = (r,g,b) * sat + (1 - sat)^2
    rgb *= sat;
    const uint8_t desat = 255 - sat;
    rgb += esp_scale8(desat, desat);
    // (r,g,b) = (r,g,b) * val
    rgb *= val;
    return rgb;
  }
};

class ESPColorCorrection {
 public:
  ESPColorCorrection() : max_brightness_(255, 255, 255, 255) {}
  void set_max_brightness(const ESPColor &max_brightness) { this->max_brightness_ = max_brightness; }
  void set_local_brightness(uint8_t local_brightness) { this->local_brightness_ = local_brightness; }
  void calculate_gamma_table(float gamma) {
    for (uint16_t i = 0; i < 256; i++) {
      // corrected = val ^ gamma
      auto corrected = static_cast<uint8_t>(roundf(255.0f * gamma_correct(i / 255.0f, gamma)));
      this->gamma_table_[i] = corrected;
    }
    if (gamma == 0.0f) {
      for (uint16_t i = 0; i < 256; i++)
        this->gamma_reverse_table_[i] = i;
      return;
    }
    for (uint16_t i = 0; i < 256; i++) {
      // val = corrected ^ (1/gamma)
      auto uncorrected = static_cast<uint8_t>(roundf(255.0f * powf(i / 255.0f, 1.0f / gamma)));
      this->gamma_reverse_table_[i] = uncorrected;
    }
  }
  inline ESPColor color_correct(ESPColor color) const ALWAYS_INLINE {
    // corrected = (uncorrected * max_brightness * local_brightness) ^ gamma
    return ESPColor(this->color_correct_red(color.red), this->color_correct_green(color.green),
                    this->color_correct_blue(color.blue), this->color_correct_white(color.white));
  }
  inline uint8_t color_correct_red(uint8_t red) const ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(red, this->max_brightness_.red), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline uint8_t color_correct_green(uint8_t green) const ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(green, this->max_brightness_.green), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline uint8_t color_correct_blue(uint8_t blue) const ALWAYS_INLINE {
    uint8_t res = esp_scale8(esp_scale8(blue, this->max_brightness_.blue), this->local_brightness_);
    return this->gamma_table_[res];
  }
  inline uint8_t color_correct_white(uint8_t white) const ALWAYS_INLINE {
    // do not scale white value with brightness
    uint8_t res = esp_scale8(white, this->max_brightness_.white);
    return this->gamma_table_[res];
  }
  inline ESPColor color_uncorrect(ESPColor color) const ALWAYS_INLINE {
    // uncorrected = corrected^(1/gamma) / (max_brightness * local_brightness)
    return ESPColor(this->color_uncorrect_red(color.red), this->color_uncorrect_green(color.green),
                    this->color_uncorrect_blue(color.blue), this->color_uncorrect_white(color.white));
  }
  inline uint8_t color_uncorrect_red(uint8_t red) const ALWAYS_INLINE {
    if (this->max_brightness_.red == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[red] * 255UL;
    uint8_t res = ((uncorrected / this->max_brightness_.red) * 255UL) / this->local_brightness_;
    return res;
  }
  inline uint8_t color_uncorrect_green(uint8_t green) const ALWAYS_INLINE {
    if (this->max_brightness_.green == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[green] * 255UL;
    uint8_t res = ((uncorrected / this->max_brightness_.green) * 255UL) / this->local_brightness_;
    return res;
  }
  inline uint8_t color_uncorrect_blue(uint8_t blue) const ALWAYS_INLINE {
    if (this->max_brightness_.blue == 0 || this->local_brightness_ == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[blue] * 255UL;
    uint8_t res = ((uncorrected / this->max_brightness_.blue) * 255UL) / this->local_brightness_;
    return res;
  }
  inline uint8_t color_uncorrect_white(uint8_t white) const ALWAYS_INLINE {
    if (this->max_brightness_.white == 0)
      return 0;
    uint16_t uncorrected = this->gamma_reverse_table_[white] * 255UL;
    uint8_t res = uncorrected / this->max_brightness_.white;
    return res;
  }

 protected:
  uint8_t gamma_table_[256];
  uint8_t gamma_reverse_table_[256];
  ESPColor max_brightness_;
  uint8_t local_brightness_{255};
};

class ESPColorView {
 public:
  inline ESPColorView(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *white, uint8_t *effect_data,
                      const ESPColorCorrection *color_correction) ALWAYS_INLINE : red_(red),
                                                                                  green_(green),
                                                                                  blue_(blue),
                                                                                  white_(white),
                                                                                  effect_data_(effect_data),
                                                                                  color_correction_(color_correction) {}
  inline const ESPColorView &operator=(const ESPColor &rhs) const ALWAYS_INLINE {
    this->set(rhs);
    return *this;
  }
  inline const ESPColorView &operator=(const ESPHSVColor &rhs) const ALWAYS_INLINE {
    this->set(rhs);
    return *this;
  }
  inline void set(const ESPColor &color) const ALWAYS_INLINE { this->set_rgbw(color.r, color.g, color.b, color.w); }
  inline void set(const ESPHSVColor &color) const ALWAYS_INLINE {
    ESPColor rgb = color.to_rgb();
    this->set_rgb(rgb.r, rgb.g, rgb.b);
  }
  inline void set_red(uint8_t red) const ALWAYS_INLINE {
    *this->red_ = this->color_correction_->color_correct_red(red);
  }
  inline void set_green(uint8_t green) const ALWAYS_INLINE {
    *this->green_ = this->color_correction_->color_correct_green(green);
  }
  inline void set_blue(uint8_t blue) const ALWAYS_INLINE {
    *this->blue_ = this->color_correction_->color_correct_blue(blue);
  }
  inline void set_white(uint8_t white) const ALWAYS_INLINE {
    if (this->white_ == nullptr)
      return;
    *this->white_ = this->color_correction_->color_correct_white(white);
  }
  inline void set_rgb(uint8_t red, uint8_t green, uint8_t blue) const ALWAYS_INLINE {
    this->set_red(red);
    this->set_green(green);
    this->set_blue(blue);
  }
  inline void set_rgbw(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) const ALWAYS_INLINE {
    this->set_rgb(red, green, blue);
    this->set_white(white);
  }
  inline void set_effect_data(uint8_t effect_data) const ALWAYS_INLINE {
    if (this->effect_data_ == nullptr)
      return;
    *this->effect_data_ = effect_data;
  }
  inline ESPColor get() const ALWAYS_INLINE {
    return ESPColor(this->get_red(), this->get_green(), this->get_blue(), this->get_white());
  }
  inline uint8_t get_red() const ALWAYS_INLINE { return this->color_correction_->color_uncorrect_red(*this->red_); }
  inline uint8_t get_green() const ALWAYS_INLINE {
    return this->color_correction_->color_uncorrect_green(*this->green_);
  }
  inline uint8_t get_blue() const ALWAYS_INLINE { return this->color_correction_->color_uncorrect_blue(*this->blue_); }
  inline uint8_t get_white() const ALWAYS_INLINE {
    if (this->white_ == nullptr)
      return 0;
    return this->color_correction_->color_uncorrect_white(*this->white_);
  }
  inline uint8_t get_effect_data() const ALWAYS_INLINE {
    if (this->effect_data_ == nullptr)
      return 0;
    return *this->effect_data_;
  }
  inline void raw_set_color_correction(const ESPColorCorrection *color_correction) ALWAYS_INLINE {
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

class AddressableLight : public LightOutput {
 public:
  virtual int32_t size() const = 0;
  virtual ESPColorView operator[](int32_t index) const = 0;
  virtual void clear_effect_data() = 0;
  bool is_effect_active() const { return this->effect_active_; }
  void set_effect_active(bool effect_active) { this->effect_active_ = effect_active; }
  void write_state(LightState *state) override {
    auto val = state->current_values;
    auto max_brightness = static_cast<uint8_t>(roundf(val.get_brightness() * val.get_state() * 255.0f));
    this->correction_.set_local_brightness(max_brightness);

    if (this->is_effect_active())
      return;

    // don't use LightState helper, gamma correction+brightness is handled by ESPColorView
    ESPColor color = ESPColor(uint8_t(roundf(val.get_red() * 255.0f)), uint8_t(roundf(val.get_green() * 255.0f)),
                              uint8_t(roundf(val.get_blue() * 255.0f)),
                              // white is not affected by brightness; so manually scale by state
                              uint8_t(roundf(val.get_white() * val.get_state() * 255.0f)));

    for (int i = 0; i < this->size(); i++) {
      (*this)[i] = color;
    }

    this->schedule_show();
  }
  void set_correction(float red, float green, float blue, float white = 1.0f) {
    this->correction_.set_max_brightness(ESPColor(uint8_t(roundf(red * 255.0f)), uint8_t(roundf(green * 255.0f)),
                                                  uint8_t(roundf(blue * 255.0f)), uint8_t(roundf(white * 255.0f))));
  }
  void setup_state(LightState *state) override { this->correction_.calculate_gamma_table(state->get_gamma_correct()); }
  void schedule_show() { this->next_show_ = true; }

 protected:
  bool should_show_() const { return this->effect_active_ || this->next_show_; }
  void mark_shown_() { this->next_show_ = false; }

  bool effect_active_{false};
  bool next_show_{true};
  ESPColorCorrection correction_{};
};

}  // namespace light
}  // namespace esphome
