#pragma once

#include <utility>
#include <vector>

#include "esphome/core/automation.h"
#include "light_effect.h"

namespace esphome {
namespace light {

inline static float random_cubic_float() {
  const float r = random_float() * 2.0f - 1.0f;
  return r * r * r;
}

/// Pulse effect.
class PulseLightEffect : public LightEffect {
 public:
  explicit PulseLightEffect(const std::string &name) : LightEffect(name) {}

  void apply() override {
    const uint32_t now = millis();
    if (now - this->last_color_change_ < this->update_interval_) {
      return;
    }
    auto call = this->state_->turn_on();
    float out = this->on_ ? this->max_brightness_ : this->min_brightness_;
    call.set_brightness_if_supported(out);
    call.set_transition_length_if_supported(this->on_ ? this->transition_on_length_ : this->transition_off_length_);
    this->on_ = !this->on_;
    // don't tell HA every change
    call.set_publish(false);
    call.set_save(false);
    call.perform();

    this->last_color_change_ = now;
  }

  void set_transition_on_length(uint32_t transition_length) { this->transition_on_length_ = transition_length; }
  void set_transition_off_length(uint32_t transition_length) { this->transition_off_length_ = transition_length; }

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

  void set_min_max_brightness(float min, float max) {
    this->min_brightness_ = min;
    this->max_brightness_ = max;
  }

 protected:
  bool on_ = false;
  uint32_t last_color_change_{0};
  uint32_t transition_on_length_{};
  uint32_t transition_off_length_{};
  uint32_t update_interval_{};
  float min_brightness_{0.0};
  float max_brightness_{1.0};
};

/// Random effect. Sets random colors every 10 seconds and slowly transitions between them.
class RandomLightEffect : public LightEffect {
 public:
  explicit RandomLightEffect(const std::string &name) : LightEffect(name) {}

  void apply() override {
    const uint32_t now = millis();
    if (now - this->last_color_change_ < this->update_interval_) {
      return;
    }

    auto color_mode = this->state_->remote_values.get_color_mode();
    auto call = this->state_->turn_on();
    bool changed = false;
    if (color_mode & ColorCapability::RGB) {
      call.set_red(random_float());
      call.set_green(random_float());
      call.set_blue(random_float());
      changed = true;
    }
    if (color_mode & ColorCapability::COLOR_TEMPERATURE) {
      float min = this->state_->get_traits().get_min_mireds();
      float max = this->state_->get_traits().get_max_mireds();
      call.set_color_temperature(min + random_float() * (max - min));
      changed = true;
    }
    if (color_mode & ColorCapability::COLD_WARM_WHITE) {
      call.set_cold_white(random_float());
      call.set_warm_white(random_float());
      changed = true;
    }
    if (!changed) {
      // only randomize brightness if there's no colored option available
      call.set_brightness(random_float());
    }
    call.set_transition_length_if_supported(this->transition_length_);
    call.set_publish(true);
    call.set_save(false);
    call.perform();

    this->last_color_change_ = now;
  }

  void set_transition_length(uint32_t transition_length) { this->transition_length_ = transition_length; }

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

 protected:
  uint32_t last_color_change_{0};
  uint32_t transition_length_{};
  uint32_t update_interval_{};
};

class LambdaLightEffect : public LightEffect {
 public:
  LambdaLightEffect(const std::string &name, std::function<void(bool initial_run)> f, uint32_t update_interval)
      : LightEffect(name), f_(std::move(f)), update_interval_(update_interval) {}

  void start() override { this->initial_run_ = true; }
  void apply() override {
    const uint32_t now = millis();
    if (now - this->last_run_ >= this->update_interval_ || this->initial_run_) {
      this->last_run_ = now;
      this->f_(this->initial_run_);
      this->initial_run_ = false;
    }
  }

 protected:
  std::function<void(bool initial_run)> f_;
  uint32_t update_interval_;
  uint32_t last_run_{0};
  bool initial_run_;
};

class AutomationLightEffect : public LightEffect {
 public:
  AutomationLightEffect(const std::string &name) : LightEffect(name) {}
  void stop() override { this->trig_->stop_action(); }
  void apply() override {
    if (!this->trig_->is_action_running()) {
      this->trig_->trigger();
    }
  }
  Trigger<> *get_trig() const { return trig_; }

 protected:
  Trigger<> *trig_{new Trigger<>};
};

struct StrobeLightEffectColor {
  LightColorValues color;
  uint32_t duration;
  uint32_t transition_length;
};

class StrobeLightEffect : public LightEffect {
 public:
  explicit StrobeLightEffect(const std::string &name) : LightEffect(name) {}
  void apply() override {
    const uint32_t now = millis();
    if (now - this->last_switch_ < this->colors_[this->at_color_].duration)
      return;

    // Switch to next color
    this->at_color_ = (this->at_color_ + 1) % this->colors_.size();
    auto color = this->colors_[this->at_color_].color;

    auto call = this->state_->turn_on();
    call.from_light_color_values(this->colors_[this->at_color_].color);

    if (!color.is_on()) {
      // Don't turn the light off, otherwise the light effect will be stopped
      call.set_brightness(0.0f);
      call.set_state(true);
    }
    call.set_publish(false);
    call.set_save(false);
    call.set_transition_length_if_supported(this->colors_[this->at_color_].transition_length);
    call.perform();
    this->last_switch_ = now;
  }

  void set_colors(const std::vector<StrobeLightEffectColor> &colors) { this->colors_ = colors; }

 protected:
  std::vector<StrobeLightEffectColor> colors_;
  uint32_t last_switch_{0};
  size_t at_color_{0};
};

class FlickerLightEffect : public LightEffect {
 public:
  explicit FlickerLightEffect(const std::string &name) : LightEffect(name) {}

  void apply() override {
    LightColorValues remote = this->state_->remote_values;
    LightColorValues current = this->state_->current_values;
    LightColorValues out;
    const float alpha = this->alpha_;
    const float beta = 1.0f - alpha;
    out.set_state(true);
    out.set_brightness(remote.get_brightness() * beta + current.get_brightness() * alpha +
                       (random_cubic_float() * this->intensity_));
    out.set_red(remote.get_red() * beta + current.get_red() * alpha + (random_cubic_float() * this->intensity_));
    out.set_green(remote.get_green() * beta + current.get_green() * alpha + (random_cubic_float() * this->intensity_));
    out.set_blue(remote.get_blue() * beta + current.get_blue() * alpha + (random_cubic_float() * this->intensity_));
    out.set_white(remote.get_white() * beta + current.get_white() * alpha + (random_cubic_float() * this->intensity_));
    out.set_cold_white(remote.get_cold_white() * beta + current.get_cold_white() * alpha +
                       (random_cubic_float() * this->intensity_));
    out.set_warm_white(remote.get_warm_white() * beta + current.get_warm_white() * alpha +
                       (random_cubic_float() * this->intensity_));

    auto call = this->state_->make_call();
    call.set_publish(false);
    call.set_save(false);
    call.set_transition_length_if_supported(0);
    call.from_light_color_values(out);
    call.set_state(true);
    call.perform();
  }

  void set_alpha(float alpha) { this->alpha_ = alpha; }
  void set_intensity(float intensity) { this->intensity_ = intensity; }

 protected:
  float intensity_{};
  float alpha_{};
};

}  // namespace light
}  // namespace esphome
