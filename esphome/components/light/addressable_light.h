#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "light_output.h"
#include "light_state.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

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
    uint32_t raw_32;
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
  inline bool is_on() ALWAYS_INLINE { return this->raw_32 != 0; }
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
  ESPColor fade_to_white(uint8_t amnt) { return ESPColor(255, 255, 255, 255) - (*this * amnt); }
  ESPColor fade_to_black(uint8_t amnt) { return *this * amnt; }
  ESPColor lighten(uint8_t delta) { return *this + delta; }
  ESPColor darken(uint8_t delta) { return *this - delta; }

  static const ESPColor BLACK;
  static const ESPColor WHITE;
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
  ESPColor to_rgb() const;
};

class ESPColorCorrection {
 public:
  ESPColorCorrection() : max_brightness_(255, 255, 255, 255) {}
  void set_max_brightness(const ESPColor &max_brightness) { this->max_brightness_ = max_brightness; }
  void set_local_brightness(uint8_t local_brightness) { this->local_brightness_ = local_brightness; }
  void calculate_gamma_table(float gamma);
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

class ESPColorSettable {
 public:
  virtual void set(const ESPColor &color) = 0;
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
    ESPColor rgb = color.to_rgb();
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
  ESPColorView &operator=(const ESPColor &rhs) {
    this->set(rhs);
    return *this;
  }
  ESPColorView &operator=(const ESPHSVColor &rhs) {
    this->set_hsv(rhs);
    return *this;
  }
  void set(const ESPColor &color) override { this->set_rgbw(color.r, color.g, color.b, color.w); }
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
  ESPColor get() const { return ESPColor(this->get_red(), this->get_green(), this->get_blue(), this->get_white()); }
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

class AddressableLight;

int32_t interpret_index(int32_t index, int32_t size);

class ESPRangeIterator;

class ESPRangeView : public ESPColorSettable {
 public:
  ESPRangeView(AddressableLight *parent, int32_t begin, int32_t an_end) : parent_(parent), begin_(begin), end_(an_end) {
    if (this->end_ < this->begin_) {
      this->end_ = this->begin_;
    }
  }

  ESPColorView operator[](int32_t index) const;
  ESPRangeIterator begin();
  ESPRangeIterator end();

  void set(const ESPColor &color) override;
  ESPRangeView &operator=(const ESPColor &rhs) {
    this->set(rhs);
    return *this;
  }
  ESPRangeView &operator=(const ESPColorView &rhs) {
    this->set(rhs.get());
    return *this;
  }
  ESPRangeView &operator=(const ESPHSVColor &rhs) {
    this->set_hsv(rhs);
    return *this;
  }
  ESPRangeView &operator=(const ESPRangeView &rhs);
  void set_red(uint8_t red) override;
  void set_green(uint8_t green) override;
  void set_blue(uint8_t blue) override;
  void set_white(uint8_t white) override;
  void set_effect_data(uint8_t effect_data) override;
  void fade_to_white(uint8_t amnt) override;
  void fade_to_black(uint8_t amnt) override;
  void lighten(uint8_t delta) override;
  void darken(uint8_t delta) override;
  int32_t size() const { return this->end_ - this->begin_; }

 protected:
  friend ESPRangeIterator;

  AddressableLight *parent_;
  int32_t begin_;
  int32_t end_;
};

class ESPRangeIterator {
 public:
  ESPRangeIterator(const ESPRangeView &range, int32_t i) : range_(range), i_(i) {}
  ESPRangeIterator operator++() {
    this->i_++;
    return *this;
  }
  bool operator!=(const ESPRangeIterator &other) const { return this->i_ != other.i_; }
  ESPColorView operator*() const;

 protected:
  ESPRangeView range_;
  int32_t i_;
};

class AddressableLight : public LightOutput, public Component {
 public:
  virtual int32_t size() const = 0;
  ESPColorView operator[](int32_t index) const { return this->get_view_internal(interpret_index(index, this->size())); }
  ESPColorView get(int32_t index) { return this->get_view_internal(interpret_index(index, this->size())); }
  virtual void clear_effect_data() = 0;
  ESPRangeView range(int32_t from, int32_t to) {
    from = interpret_index(from, this->size());
    to = interpret_index(to, this->size());
    return ESPRangeView(this, from, to);
  }
  ESPRangeView all() { return ESPRangeView(this, 0, this->size()); }
  ESPRangeIterator begin() { return this->all().begin(); }
  ESPRangeIterator end() { return this->all().end(); }
  void shift_left(int32_t amnt) {
    if (amnt < 0) {
      this->shift_right(-amnt);
      return;
    }
    if (amnt > this->size())
      amnt = this->size();
    this->range(0, -amnt) = this->range(amnt, this->size());
  }
  void shift_right(int32_t amnt) {
    if (amnt < 0) {
      this->shift_left(-amnt);
      return;
    }
    if (amnt > this->size())
      amnt = this->size();
    this->range(amnt, this->size()) = this->range(0, -amnt);
  }
  bool is_effect_active() const { return this->effect_active_; }
  void set_effect_active(bool effect_active) { this->effect_active_ = effect_active; }
  void write_state(LightState *state) override;
  void set_correction(float red, float green, float blue, float white = 1.0f) {
    this->correction_.set_max_brightness(ESPColor(uint8_t(roundf(red * 255.0f)), uint8_t(roundf(green * 255.0f)),
                                                  uint8_t(roundf(blue * 255.0f)), uint8_t(roundf(white * 255.0f))));
  }
  void setup_state(LightState *state) override {
    this->correction_.calculate_gamma_table(state->get_gamma_correct());
    this->state_parent_ = state;
  }
  void schedule_show() { this->next_show_ = true; }

#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  void call_setup() override;

 protected:
  bool should_show_() const { return this->effect_active_ || this->next_show_; }
  void mark_shown_() {
    this->next_show_ = false;
#ifdef USE_POWER_SUPPLY
    for (auto c : *this) {
      if (c.get().is_on()) {
        this->power_.request();
        return;
      }
    }
    this->power_.unrequest();
#endif
  }
  virtual ESPColorView get_view_internal(int32_t index) const = 0;

  bool effect_active_{false};
  bool next_show_{true};
  ESPColorCorrection correction_{};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif
  LightState *state_parent_{nullptr};
  float last_transition_progress_{0.0f};
  float accumulated_alpha_{0.0f};
};

}  // namespace light
}  // namespace esphome
