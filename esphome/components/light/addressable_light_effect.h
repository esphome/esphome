#pragma once

#include <math.h>
#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/gradient/gradient.h"

namespace esphome {
namespace light {
using esphome::gradient::Gradient;

inline static int16_t sin16_c(uint16_t theta) {
  static const uint16_t BASE[] = {0, 6393, 12539, 18204, 23170, 27245, 30273, 32137};
  static const uint8_t SLOPE[] = {49, 48, 44, 38, 31, 23, 14, 4};
  uint16_t offset = (theta & 0x3FFF) >> 3;  // 0..2047
  if (theta & 0x4000)
    offset = 2047 - offset;
  uint8_t section = offset / 256;  // 0..7
  uint16_t b = BASE[section];
  uint8_t m = SLOPE[section];
  uint8_t secoffset8 = uint8_t(offset) / 2;
  uint16_t mx = m * secoffset8;
  int16_t y = mx + b;
  if (theta & 0x8000)
    return -y;
  return y;
}
inline static uint8_t half_sin8(uint8_t v) { return sin16_c(uint16_t(v) * 128u) >> 8; }

class AddressableLightEffect : public LightEffect {
 public:
  explicit AddressableLightEffect(const std::string &name) : LightEffect(name) {}
  void start_internal() override {
    this->get_addressable_()->set_effect_active(true);
    this->get_addressable_()->clear_effect_data();
    this->start();
  }
  void stop() override { this->get_addressable_()->set_effect_active(false); }
  virtual void apply(AddressableLight &it, const ESPColor &current_color) = 0;
  void apply() override {
    LightColorValues color = this->state_->remote_values;
    // not using any color correction etc. that will be handled by the addressable layer
    ESPColor current_color =
        ESPColor(static_cast<uint8_t>(color.get_red() * 255), static_cast<uint8_t>(color.get_green() * 255),
                 static_cast<uint8_t>(color.get_blue() * 255), static_cast<uint8_t>(color.get_white() * 255));
    this->apply(*this->get_addressable_(), current_color);
  }

 protected:
  AddressableLight *get_addressable_() const { return (AddressableLight *) this->state_->get_output(); }
};

class AddressableLambdaLightEffect : public AddressableLightEffect {
 public:
  AddressableLambdaLightEffect(const std::string &name, const std::function<void(AddressableLight &, ESPColor)> &f,
                               uint32_t update_interval)
      : AddressableLightEffect(name), f_(f), update_interval_(update_interval) {}
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_run_ >= this->update_interval_) {
      this->last_run_ = now;
      this->f_(it, current_color);
    }
  }

 protected:
  std::function<void(AddressableLight &, ESPColor)> f_;
  uint32_t update_interval_;
  uint32_t last_run_{0};
};

class AddressableRainbowLightEffect : public AddressableLightEffect {
 public:
  explicit AddressableRainbowLightEffect(const std::string &name) : AddressableLightEffect(name) {}
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    ESPHSVColor hsv;
    hsv.value = 255;
    hsv.saturation = 240;
    uint16_t hue = (millis() * this->speed_) % 0xFFFF;
    const uint16_t add = 0xFFFF / this->width_;
    for (auto var : it) {
      hsv.hue = hue >> 8;
      var = hsv;
      hue += add;
    }
  }
  void set_speed(uint32_t speed) { this->speed_ = speed; }
  void set_width(uint16_t width) { this->width_ = width; }

 protected:
  uint32_t speed_{10};
  uint16_t width_{50};
};

struct AddressableColorWipeEffectColor {
  uint8_t r, g, b, w;
  bool random;
  size_t num_leds;
};

class AddressableColorWipeEffect : public AddressableLightEffect {
 public:
  explicit AddressableColorWipeEffect(const std::string &name) : AddressableLightEffect(name) {}
  void set_colors(const std::vector<AddressableColorWipeEffectColor> &colors) { this->colors_ = colors; }
  void set_add_led_interval(uint32_t add_led_interval) { this->add_led_interval_ = add_led_interval; }
  void set_reverse(bool reverse) { this->reverse_ = reverse; }
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_add_ < this->add_led_interval_)
      return;
    this->last_add_ = now;
    if (this->reverse_)
      it.shift_left(1);
    else
      it.shift_right(1);
    const AddressableColorWipeEffectColor color = this->colors_[this->at_color_];
    const ESPColor esp_color = ESPColor(color.r, color.g, color.b, color.w);
    if (this->reverse_)
      it[-1] = esp_color;
    else
      it[0] = esp_color;
    if (++this->leds_added_ >= color.num_leds) {
      this->leds_added_ = 0;
      this->at_color_ = (this->at_color_ + 1) % this->colors_.size();
      AddressableColorWipeEffectColor &new_color = this->colors_[this->at_color_];
      if (new_color.random) {
        ESPColor c = ESPColor::random_color();
        new_color.r = c.r;
        new_color.g = c.g;
        new_color.b = c.b;
      }
    }
  }

 protected:
  std::vector<AddressableColorWipeEffectColor> colors_;
  size_t at_color_{0};
  uint32_t last_add_{0};
  uint32_t add_led_interval_{};
  size_t leds_added_{0};
  bool reverse_{};
};

class AddressableScanEffect : public AddressableLightEffect {
 public:
  explicit AddressableScanEffect(const std::string &name) : AddressableLightEffect(name) {}
  void set_move_interval(uint32_t move_interval) { this->move_interval_ = move_interval; }
  void set_scan_width(uint32_t scan_width) { this->scan_width_ = scan_width; }
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    it.all() = ESPColor::BLACK;

    for (auto i = 0; i < this->scan_width_; i++) {
      it[this->at_led_ + i] = current_color;
    }

    const uint32_t now = millis();
    if (now - this->last_move_ > this->move_interval_) {
      if (direction_) {
        this->at_led_++;
        if (this->at_led_ == it.size() - this->scan_width_)
          this->direction_ = false;
      } else {
        this->at_led_--;
        if (this->at_led_ == 0)
          this->direction_ = true;
      }
      this->last_move_ = now;
    }
  }

 protected:
  uint32_t move_interval_{};
  uint32_t scan_width_{1};
  uint32_t last_move_{0};
  int at_led_{0};
  bool direction_{true};
};

class AddressableTwinkleEffect : public AddressableLightEffect {
 public:
  explicit AddressableTwinkleEffect(const std::string &name) : AddressableLightEffect(name) {}
  void apply(AddressableLight &addressable, const ESPColor &current_color) override {
    const uint32_t now = millis();
    uint8_t pos_add = 0;
    if (now - this->last_progress_ > this->progress_interval_) {
      const uint32_t pos_add32 = (now - this->last_progress_) / this->progress_interval_;
      pos_add = pos_add32;
      this->last_progress_ += pos_add32 * this->progress_interval_;
    }
    for (auto view : addressable) {
      if (view.get_effect_data() != 0) {
        const uint8_t sine = half_sin8(view.get_effect_data());
        view = current_color * sine;
        const uint8_t new_pos = view.get_effect_data() + pos_add;
        if (new_pos < view.get_effect_data())
          view.set_effect_data(0);
        else
          view.set_effect_data(new_pos);
      } else {
        view = ESPColor::BLACK;
      }
    }
    while (random_float() < this->twinkle_probability_) {
      const size_t pos = random_uint32() % addressable.size();
      if (addressable[pos].get_effect_data() != 0)
        continue;
      addressable[pos].set_effect_data(1);
    }
  }
  void set_twinkle_probability(float twinkle_probability) { this->twinkle_probability_ = twinkle_probability; }
  void set_progress_interval(uint32_t progress_interval) { this->progress_interval_ = progress_interval; }

 protected:
  float twinkle_probability_{0.05f};
  uint32_t progress_interval_{4};
  uint32_t last_progress_{0};
};

class AddressableRandomTwinkleEffect : public AddressableLightEffect {
 public:
  explicit AddressableRandomTwinkleEffect(const std::string &name) : AddressableLightEffect(name) {}
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    const uint32_t now = millis();
    uint8_t pos_add = 0;
    if (now - this->last_progress_ > this->progress_interval_) {
      pos_add = (now - this->last_progress_) / this->progress_interval_;
      this->last_progress_ = now;
    }
    uint8_t subsine = ((8 * (now - this->last_progress_)) / this->progress_interval_) & 0b111;
    for (auto view : it) {
      if (view.get_effect_data() != 0) {
        const uint8_t x = (view.get_effect_data() >> 3) & 0b11111;
        const uint8_t color = view.get_effect_data() & 0b111;
        const uint16_t sine = half_sin8((x << 3) | subsine);
        if (color == 0) {
          view = current_color * sine;
        } else {
          view = ESPColor(((color >> 2) & 1) * sine, ((color >> 1) & 1) * sine, ((color >> 0) & 1) * sine);
        }
        const uint8_t new_x = x + pos_add;
        if (new_x > 0b11111)
          view.set_effect_data(0);
        else
          view.set_effect_data((new_x << 3) | color);
      } else {
        view = ESPColor(0, 0, 0, 0);
      }
    }
    while (random_float() < this->twinkle_probability_) {
      const size_t pos = random_uint32() % it.size();
      if (it[pos].get_effect_data() != 0)
        continue;
      const uint8_t color = random_uint32() & 0b111;
      it[pos].set_effect_data(0b1000 | color);
    }
  }
  void set_twinkle_probability(float twinkle_probability) { this->twinkle_probability_ = twinkle_probability; }
  void set_progress_interval(uint32_t progress_interval) { this->progress_interval_ = progress_interval; }

 protected:
  float twinkle_probability_{};
  uint32_t progress_interval_{};
  uint32_t last_progress_{0};
};

class AddressableFireworksEffect : public AddressableLightEffect {
 public:
  explicit AddressableFireworksEffect(const std::string &name) : AddressableLightEffect(name) {}
  void start() override {
    auto &it = *this->get_addressable_();
    it.all() = ESPColor::BLACK;
  }
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_update_ < this->update_interval_)
      return;
    this->last_update_ = now;
    // "invert" the fade out parameter so that higher values make fade out faster
    const uint8_t fade_out_mult = 255u - this->fade_out_rate_;
    for (auto view : it) {
      ESPColor target = view.get() * fade_out_mult;
      if (target.r < 64)
        target *= 170;
      view = target;
    }
    int last = it.size() - 1;
    it[0].set(it[0].get() + (it[1].get() * 128));
    for (int i = 1; i < last; i++) {
      it[i] = (it[i - 1].get() * 64) + it[i].get() + (it[i + 1].get() * 64);
    }
    it[last] = it[last].get() + (it[last - 1].get() * 128);
    if (random_float() < this->spark_probability_) {
      const size_t pos = random_uint32() % it.size();
      if (this->use_random_color_) {
        it[pos] = ESPColor::random_color();
      } else {
        it[pos] = current_color;
      }
    }
  }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
  void set_spark_probability(float spark_probability) { this->spark_probability_ = spark_probability; }
  void set_use_random_color(bool random_color) { this->use_random_color_ = random_color; }
  void set_fade_out_rate(uint8_t fade_out_rate) { this->fade_out_rate_ = fade_out_rate; }

 protected:
  uint8_t fade_out_rate_{};
  uint32_t update_interval_{};
  uint32_t last_update_{0};
  float spark_probability_{};
  bool use_random_color_{};
};

class AddressableFlickerEffect : public AddressableLightEffect {
 public:
  explicit AddressableFlickerEffect(const std::string &name) : AddressableLightEffect(name) {}
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    const uint32_t now = millis();
    const uint8_t intensity = this->intensity_;
    const uint8_t inv_intensity = 255 - intensity;
    if (now - this->last_update_ < this->update_interval_)
      return;

    this->last_update_ = now;
    fast_random_set_seed(random_uint32());
    for (auto var : it) {
      const uint8_t flicker = fast_random_8() % intensity;
      // scale down by random factor
      var = var.get() * (255 - flicker);

      // slowly fade back to "real" value
      var = (var.get() * inv_intensity) + (current_color * intensity);
    }
  }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
  void set_intensity(float intensity) { this->intensity_ = static_cast<uint8_t>(roundf(intensity * 255.0f)); }

 protected:
  uint32_t update_interval_{16};
  uint32_t last_update_{0};
  uint8_t intensity_{13};
};


#define CP(x) ((1.0/255.0)*(x))

class AddressableGradientEffect : public AddressableLightEffect {
 public:
  explicit AddressableGradientEffect(const std::string &name) : AddressableLightEffect(name) {}
  void set_gradient(const Gradient *gradient) { this->gradient_ = gradient; }
  void set_move_interval(uint32_t move_interval) { this->move_interval_ = move_interval; }
  void set_length(uint32_t length) { this->length_ = length; this->step_ = 1.0/length; }
  void set_flip(bool flip) { this->flip_ = flip; }
  void set_use_white(bool use_white) { this->use_white_ = use_white; }
  void set_reverse(bool reverse) { this->reverse_ = reverse; }
  void apply(AddressableLight &it, const ESPColor &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_add_ < this->move_interval_ || this->gradient_ == nullptr)
      return;
    
    this->last_add_ = now;
    if (this->reverse_)
      it.shift_left(1);
    else
      it.shift_right(1);
    //const AddressableGradientEffectColor color = this->colors_[this->at_color_];

    // ESP_LOGD("custom", "gr %p grp %p", this->gradient_, (void *)this->gradient_->get_gradient());
    ESPColor color = this->gradient_->color(this->pos_);
    ESP_LOGD("custom", "Color at@%f is: r:%d g:%d b:%d w%d", this->pos_,  color.r, color.g, color.b, color.w);

    // FIXME: the white channel on rgbw is handled seperatly, but we should apply the same
    // brightness as the rgb leds here, not the white value
    if (this->use_white_) {
      float brightness = this->state_->remote_values.get_brightness();
      uint8_t white = min(color.r, min(color.g, color.b));
      if (white) {
        color.w = white * brightness;
        color.r = color.r - white;
        color.g = color.g - white;
        color.b = color.b - white;
      }
    }
    
    if (this->reverse_)
      it[-1] = color;
    else
      it[0] = color;

    if (this->flip_) {
      if (this->forward_ == true && (this->pos_ + this->step_  >= 1.0f)) {
        this->forward_ = false;
        this->pos_ = 1.0f;
      } else if (this->forward_ == false && (this->pos_ - this->step_  <= 0.0f)) {
        this->forward_ = true;
        this->pos_ = 0.0f;
      }
    }
    if (this->forward_ == true) {
      this->pos_ = this->pos_ + this->step_;
      if(this->pos_ > 1.0) {
        this->pos_ = 0.0;
      }
    } else {
      this->pos_ = max(this->pos_ - this->step_, 0.0f);
      if(this->pos_ < 0.0) {
        this->pos_ = 1.0;
      }
    }
    //AddressableColorWipeEffectColor &new_color = this->colors_[this->at_color_];
  }

  // ESPColor color(float x) {
  //       /* for seg in self.segs:
  //           if seg.l <= x <= seg.r:
  //               break
  //       else:
  //           # No segment applies! Return black I guess.
  //           return (0,0,0)

  //       # Normalize the segment geometry.
  //       mid = (seg.m - seg.l)/(seg.r - seg.l)
  //       pos = (x - seg.l)/(seg.r - seg.l)
        
  //       # Assume linear (most common, and needed by most others).
  //       if pos <= mid:
  //           f = pos/mid/2
  //       else:
  //           f = (pos - mid)/(1 - mid)/2 + 0.5

  //       # Find the correct interpolation factor.
  //       if seg.fn == 1:   # Curved
  //           f = math.pow(pos, math.log(0.5) / math.log(mid));
  //       elif seg.fn == 2:   # Sinusoidal
  //           f = (math.sin((-math.pi/2) + math.pi*f) + 1)/2
  //       elif seg.fn == 3:   # Spherical increasing
  //           f -= 1
  //           f = math.sqrt(1 - f*f)
  //       elif seg.fn == 4:   # Spherical decreasing
  //           f = 1 - math.sqrt(1 - f*f);

  //       # Interpolate the colors
  //       if seg.space == 0:
  //           c = (
  //               seg.rl + (seg.rr-seg.rl) * f,
  //               seg.gl + (seg.gr-seg.gl) * f,
  //               seg.bl + (seg.br-seg.bl) * f
  //               )
  //       elif seg.space in (1,2):
  //           hl, sl, vl = colorsys.rgb_to_hsv(seg.rl, seg.gl, seg.bl)
  //           hr, sr, vr = colorsys.rgb_to_hsv(seg.rr, seg.gr, seg.br)

  //           if seg.space == 1 and hr < hl:
  //               hr += 1
  //           elif seg.space == 2 and hr > hl:
  //               hr -= 1

  //           c = colorsys.hsv_to_rgb(
  //               (hl + (hr-hl) * f) % 1.0,
  //               sl + (sr-sl) * f,
  //               vl + (vr-vl) * f
  //               )
  //       return c
  //        */
  //   //ESP_LOGF("x point:", x);
  //   ESP_LOGD("custom", "Pos x: %f", x);
  //   AddressableGradientEffectColor seg;
  //   ESPColor c = ESPColor(0,0,0);
  //   bool ok = false;
  //   int i=0;
  //   for (i=0; i < this->gradient_.size(); i++) {
  //     seg = this->gradient_[i];
  //     ESP_LOGD("custom", "Test i: %d l: %lf r: %lf", i, seg.l, seg.r);
  //     if(seg.l <= x && x <= seg.r) {
  //       ok = true;
  //       break;
  //     }
  //   }
  //   // for(auto seg: this->gradient_) {
  //   //   if(seg.l <= x && x <= seg.r) {
  //   //     ok = true;
  //   //     break;
  //   //   }
  //   // }
  //   if(!ok){
  //     ESP_LOGD("custom", "!ok");
  //     return ESPColor::BLACK;
  //   }

  //   float mid = (seg.m - seg.l)/(seg.r - seg.l);
  //   float pos = (x - seg.l)/(seg.r - seg.l);
  //   float f;

  //   // Assume linear (most common, and needed by most others).
  //   if (pos <= mid) {
  //     f = (pos/mid/2.0);
  //   } else {
  //     f = (pos - mid)/(1.0 - mid)/2.0 + 0.5;
  //   }

  //   if(seg.fn == 1) {   // Curved
  //     f = pow(pos,(log(0.5) / log(mid)));
  //   } else if (seg.fn == 2) {  // Sinusoidal
  //     f = (sin((-M_PI/2.0) + M_PI*f) + 1.0)/2.0;
  //   } else if (seg.fn == 3) {   // Spherical increasing
  //     f -= 1.0;
  //     f = sqrt(1.0 - f*f);
  //   } else if (seg.fn == 4) {   // Spherical decreasing
  //     f = 1.0 - sqrt(1.0 - f*f);
  //   }

  //   uint8_t xf = 255*x;
  //   ESP_LOGD("custom", "i:%d x: %f xf: %d mid:%f pos:%f f:%f",i, x, xf, mid, pos, f);
  //   ESP_LOGD("custom", "seg.rl: %f seg.gl: %f seg.bl: %f seg.rr: %f seg.gr: %f seg.br: %f",
  //           seg.rl, seg.gl, seg.bl, seg.rr, seg.gr, seg.br);
  //   ESP_LOGD("custom", "seg.l: %f seg.m: %f seg.r: %f seg.fn: %d seg.space: %d",
  //           seg.l, seg.m, seg.r, seg.fn, seg.space);
    
  //   ESP_LOGD("custom", "finish r:%d g:%d b:%d",
  //       (uint8_t)((seg.rl + (seg.rr-seg.rl) * f)*255),
  //       (uint8_t)((seg.gl + (seg.gr-seg.gl) * f)*255),
  //       (uint8_t)((seg.bl + (seg.br-seg.bl) * f)*255));

  //   if (seg.space == 0) {
  //     c = ESPColor(
  //       (uint8_t)((seg.rl + (seg.rr-seg.rl) * f)*255),
  //       (uint8_t)((seg.gl + (seg.gr-seg.gl) * f)*255),
  //       (uint8_t)((seg.bl + (seg.br-seg.bl) * f)*255)
  //     );
      
  //   } else if (seg.space == 1 || seg.space == 2) {
  //     float hl, sl, vl, hr, sr, vr;
  //     ESPHSVColor::rgb2hsv(seg.rl, seg.gl, seg.bl, hl, sl, vl);
  //     ESPHSVColor::rgb2hsv(seg.rr, seg.gr, seg.br, hr, sr, vr);

  //     if (seg.space == 1 && hr < hl) {
  //       hr += 1;
  //     } else if ( seg.space == 2 and hr > hl) {
  //       hr -= 1;
  //     }
  //     //float h = l.h + (r.h-l.h) * f;
  //     ESP_LOGD("custom", "xx %f %f %d", (hl + (hr-hl) * f), fmod(hl + (hr-hl) * f, 1.0), (uint8_t)(fmod(hl + (hr-hl) * f, 360)*(255.0/360.0)));
  //     c = ESPHSVColor(
  //           (uint8_t)(fmod(hl + (hr-hl) * f, 360.0)*(255.0/360.0)),
  //           (uint8_t)((sl + (sr-sl) * f)*255),
  //           (uint8_t)((vl + (vr-vl) * f)*255)
  //           ).to_rgb();
      
  //   }
  //   return c;
 

 protected:
  //std::vector<AddressableGradientEffectColor> gradient_;
  const Gradient* gradient_{nullptr};
  float step_{0};
  float pos_{0};
  uint32_t last_add_{0};
  uint32_t move_interval_{};
  //size_t leds_added_{0};
  bool reverse_{};
  bool flip_{};
  bool use_white_{};
  bool forward_{true};
  uint32_t length_{0};
  float lengthfrac_{0.0};
};

}  // namespace light
}  // namespace esphome
