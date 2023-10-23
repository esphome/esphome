#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace light {

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
  virtual void apply(AddressableLight &it, const Color &current_color) = 0;
  void apply() override {
    // not using any color correction etc. that will be handled by the addressable layer through ESPColorCorrection
    Color current_color = color_from_light_color_values(this->state_->remote_values);
    this->apply(*this->get_addressable_(), current_color);
  }

 protected:
  AddressableLight *get_addressable_() const { return (AddressableLight *) this->state_->get_output(); }
};

class AddressableLambdaLightEffect : public AddressableLightEffect {
 public:
  AddressableLambdaLightEffect(const std::string &name,
                               std::function<void(AddressableLight &, Color, bool initial_run)> f,
                               uint32_t update_interval)
      : AddressableLightEffect(name), f_(std::move(f)), update_interval_(update_interval) {}
  void start() override { this->initial_run_ = true; }
  void apply(AddressableLight &it, const Color &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_run_ >= this->update_interval_) {
      this->last_run_ = now;
      this->f_(it, current_color, this->initial_run_);
      this->initial_run_ = false;
      it.schedule_show();
    }
  }

 protected:
  std::function<void(AddressableLight &, Color, bool initial_run)> f_;
  uint32_t update_interval_;
  uint32_t last_run_{0};
  bool initial_run_;
};

class AddressableRainbowLightEffect : public AddressableLightEffect {
 public:
  explicit AddressableRainbowLightEffect(const std::string &name) : AddressableLightEffect(name) {}
  void apply(AddressableLight &it, const Color &current_color) override {
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
    it.schedule_show();
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
  void apply(AddressableLight &it, const Color &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_add_ < this->add_led_interval_)
      return;
    this->last_add_ = now;
    if (this->reverse_)
      it.shift_left(1);
    else
      it.shift_right(1);
    const AddressableColorWipeEffectColor color = this->colors_[this->at_color_];
    const Color esp_color = Color(color.r, color.g, color.b, color.w);
    if (this->reverse_)
      it[-1] = esp_color;
    else
      it[0] = esp_color;
    if (++this->leds_added_ >= color.num_leds) {
      this->leds_added_ = 0;
      this->at_color_ = (this->at_color_ + 1) % this->colors_.size();
      AddressableColorWipeEffectColor &new_color = this->colors_[this->at_color_];
      if (new_color.random) {
        Color c = Color::random_color();
        new_color.r = c.r;
        new_color.g = c.g;
        new_color.b = c.b;
      }
    }
    it.schedule_show();
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
  void apply(AddressableLight &it, const Color &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_move_ < this->move_interval_)
      return;

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

    it.all() = Color::BLACK;
    for (uint32_t i = 0; i < this->scan_width_; i++) {
      it[this->at_led_ + i] = current_color;
    }

    it.schedule_show();
  }

 protected:
  uint32_t move_interval_{};
  uint32_t scan_width_{1};
  uint32_t last_move_{0};
  uint32_t at_led_{0};
  bool direction_{true};
};

class AddressableTwinkleEffect : public AddressableLightEffect {
 public:
  explicit AddressableTwinkleEffect(const std::string &name) : AddressableLightEffect(name) {}
  void apply(AddressableLight &addressable, const Color &current_color) override {
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
        view = Color::BLACK;
      }
    }
    while (random_float() < this->twinkle_probability_) {
      const size_t pos = random_uint32() % addressable.size();
      if (addressable[pos].get_effect_data() != 0)
        continue;
      addressable[pos].set_effect_data(1);
    }
    addressable.schedule_show();
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
  void apply(AddressableLight &it, const Color &current_color) override {
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
          view = Color(((color >> 2) & 1) * sine, ((color >> 1) & 1) * sine, ((color >> 0) & 1) * sine);
        }
        const uint8_t new_x = x + pos_add;
        if (new_x > 0b11111)
          view.set_effect_data(0);
        else
          view.set_effect_data((new_x << 3) | color);
      } else {
        view = Color(0, 0, 0, 0);
      }
    }
    while (random_float() < this->twinkle_probability_) {
      const size_t pos = random_uint32() % it.size();
      if (it[pos].get_effect_data() != 0)
        continue;
      const uint8_t color = random_uint32() & 0b111;
      it[pos].set_effect_data(0b1000 | color);
    }
    it.schedule_show();
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
    it.all() = Color::BLACK;
  }
  void apply(AddressableLight &it, const Color &current_color) override {
    const uint32_t now = millis();
    if (now - this->last_update_ < this->update_interval_)
      return;
    this->last_update_ = now;
    // "invert" the fade out parameter so that higher values make fade out faster
    const uint8_t fade_out_mult = 255u - this->fade_out_rate_;
    for (auto view : it) {
      Color target = view.get() * fade_out_mult;
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
        it[pos] = Color::random_color();
      } else {
        it[pos] = current_color;
      }
    }
    it.schedule_show();
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
  void apply(AddressableLight &it, const Color &current_color) override {
    const uint32_t now = millis();
    const uint8_t intensity = this->intensity_;
    const uint8_t inv_intensity = 255 - intensity;
    if (now - this->last_update_ < this->update_interval_)
      return;

    this->last_update_ = now;
    uint32_t rng_state = random_uint32();
    for (auto var : it) {
      rng_state = (rng_state * 0x9E3779B9) + 0x9E37;
      const uint8_t flicker = (rng_state & 0xFF) % intensity;
      // scale down by random factor
      var = var.get() * (255 - flicker);

      // slowly fade back to "real" value
      var = (var.get() * inv_intensity) + (current_color * intensity);
    }
    it.schedule_show();
  }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
  void set_intensity(float intensity) { this->intensity_ = to_uint8_scale(intensity); }

 protected:
  uint32_t update_interval_{16};
  uint32_t last_update_{0};
  uint8_t intensity_{13};
};

}  // namespace light
}  // namespace esphome
