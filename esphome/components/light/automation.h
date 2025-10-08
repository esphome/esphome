#pragma once

#include "esphome/core/automation.h"
#include "light_state.h"
#include "addressable_light.h"

namespace esphome {
namespace light {

enum class LimitMode { CLAMP, DO_NOTHING };

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(LightState *state) : state_(state) {}

  TEMPLATABLE_VALUE(uint32_t, transition_length)

  void play(Ts... x) override {
    auto call = this->state_->toggle();
    call.set_transition_length(this->transition_length_.optional_value(x...));
    call.perform();
  }

 protected:
  LightState *state_;
};

template<typename... Ts> class LightControlAction : public Action<Ts...> {
 public:
  explicit LightControlAction(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(ColorMode, color_mode)
  TEMPLATABLE_VALUE(bool, state)
  TEMPLATABLE_VALUE(uint32_t, transition_length)
  TEMPLATABLE_VALUE(uint32_t, flash_length)
  TEMPLATABLE_VALUE(float, brightness)
  TEMPLATABLE_VALUE(float, color_brightness)
  TEMPLATABLE_VALUE(float, red)
  TEMPLATABLE_VALUE(float, green)
  TEMPLATABLE_VALUE(float, blue)
  TEMPLATABLE_VALUE(float, white)
  TEMPLATABLE_VALUE(float, color_temperature)
  TEMPLATABLE_VALUE(float, cold_white)
  TEMPLATABLE_VALUE(float, warm_white)
  TEMPLATABLE_VALUE(std::string, effect)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();
    call.set_color_mode(this->color_mode_.optional_value(x...));
    call.set_state(this->state_.optional_value(x...));
    call.set_brightness(this->brightness_.optional_value(x...));
    call.set_color_brightness(this->color_brightness_.optional_value(x...));
    call.set_red(this->red_.optional_value(x...));
    call.set_green(this->green_.optional_value(x...));
    call.set_blue(this->blue_.optional_value(x...));
    call.set_white(this->white_.optional_value(x...));
    call.set_color_temperature(this->color_temperature_.optional_value(x...));
    call.set_cold_white(this->cold_white_.optional_value(x...));
    call.set_warm_white(this->warm_white_.optional_value(x...));
    call.set_effect(this->effect_.optional_value(x...));
    call.set_flash_length(this->flash_length_.optional_value(x...));
    call.set_transition_length(this->transition_length_.optional_value(x...));
    call.perform();
  }

 protected:
  LightState *parent_;
};

template<typename... Ts> class DimRelativeAction : public Action<Ts...> {
 public:
  explicit DimRelativeAction(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, relative_brightness)
  TEMPLATABLE_VALUE(uint32_t, transition_length)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();
    float rel = this->relative_brightness_.value(x...);
    float cur;
    this->parent_->remote_values.as_brightness(&cur);
    if ((limit_mode_ == LimitMode::DO_NOTHING) && ((cur < min_brightness_) || (cur > max_brightness_))) {
      return;
    }
    float new_brightness = clamp(cur + rel, min_brightness_, max_brightness_);
    call.set_state(new_brightness != 0.0f);
    call.set_brightness(new_brightness);

    call.set_transition_length(this->transition_length_.optional_value(x...));
    call.perform();
  }

  void set_min_max_brightness(float min, float max) {
    this->min_brightness_ = min;
    this->max_brightness_ = max;
  }

  void set_limit_mode(LimitMode limit_mode) { this->limit_mode_ = limit_mode; }

 protected:
  LightState *parent_;
  float min_brightness_{0.0};
  float max_brightness_{1.0};
  LimitMode limit_mode_{LimitMode::CLAMP};
};

template<typename... Ts> class LightIsOnCondition : public Condition<Ts...> {
 public:
  explicit LightIsOnCondition(LightState *state) : state_(state) {}
  bool check(Ts... x) override { return this->state_->current_values.is_on(); }

 protected:
  LightState *state_;
};
template<typename... Ts> class LightIsOffCondition : public Condition<Ts...> {
 public:
  explicit LightIsOffCondition(LightState *state) : state_(state) {}
  bool check(Ts... x) override { return !this->state_->current_values.is_on(); }

 protected:
  LightState *state_;
};

class LightTurnOnTrigger : public Trigger<> {
 public:
  LightTurnOnTrigger(LightState *a_light) {
    a_light->add_new_remote_values_callback([this, a_light]() {
      // using the remote value because of transitions we need to trigger as early as possible
      auto is_on = a_light->remote_values.is_on();
      // only trigger when going from off to on
      auto should_trigger = is_on && !this->last_on_;
      // Set new state immediately so that trigger() doesn't devolve
      // into infinite loop
      this->last_on_ = is_on;
      if (should_trigger) {
        this->trigger();
      }
    });
    this->last_on_ = a_light->current_values.is_on();
  }

 protected:
  bool last_on_;
};

class LightTurnOffTrigger : public Trigger<> {
 public:
  LightTurnOffTrigger(LightState *a_light) {
    a_light->add_new_target_state_reached_callback([this, a_light]() {
      auto is_on = a_light->current_values.is_on();
      // only trigger when going from on to off
      if (!is_on) {
        this->trigger();
      }
    });
  }
};

class LightStateTrigger : public Trigger<> {
 public:
  LightStateTrigger(LightState *a_light) {
    a_light->add_new_remote_values_callback([this]() { this->trigger(); });
  }
};

// This is slightly ugly, but we can't log in headers, and can't make this a static method on AddressableSet
// due to the template. It's just a temporary warning anyway.
void addressableset_warn_about_scale(const char *field);

template<typename... Ts> class AddressableSet : public Action<Ts...> {
 public:
  explicit AddressableSet(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int32_t, range_from)
  TEMPLATABLE_VALUE(int32_t, range_to)
  TEMPLATABLE_VALUE(float, color_brightness)
  TEMPLATABLE_VALUE(float, red)
  TEMPLATABLE_VALUE(float, green)
  TEMPLATABLE_VALUE(float, blue)
  TEMPLATABLE_VALUE(float, white)

  void play(Ts... x) override {
    auto *out = (AddressableLight *) this->parent_->get_output();
    int32_t range_from = interpret_index(this->range_from_.value_or(x..., 0), out->size());
    if (range_from < 0 || range_from >= out->size())
      range_from = 0;

    int32_t range_to = interpret_index(this->range_to_.value_or(x..., out->size() - 1) + 1, out->size());
    if (range_to < 0 || range_to >= out->size())
      range_to = out->size();

    uint8_t color_brightness =
        to_uint8_scale(this->color_brightness_.value_or(x..., this->parent_->remote_values.get_color_brightness()));
    auto range = out->range(range_from, range_to);
    if (this->red_.has_value())
      range.set_red(esp_scale8(to_uint8_compat(this->red_.value(x...), "red"), color_brightness));
    if (this->green_.has_value())
      range.set_green(esp_scale8(to_uint8_compat(this->green_.value(x...), "green"), color_brightness));
    if (this->blue_.has_value())
      range.set_blue(esp_scale8(to_uint8_compat(this->blue_.value(x...), "blue"), color_brightness));
    if (this->white_.has_value())
      range.set_white(to_uint8_compat(this->white_.value(x...), "white"));
    out->schedule_show();
  }

 protected:
  LightState *parent_;

  // Historically, this action required uint8_t (0-255) for RGBW values from lambdas. Keep compatibility.
  static inline uint8_t to_uint8_compat(float value, const char *field) {
    if (value > 1.0f) {
      addressableset_warn_about_scale(field);
      return static_cast<uint8_t>(value);
    }
    return to_uint8_scale(value);
  }
};

}  // namespace light
}  // namespace esphome
