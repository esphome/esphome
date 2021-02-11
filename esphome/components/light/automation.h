#pragma once

#include "esphome/core/automation.h"
#include "light_state.h"
#include "addressable_light.h"

namespace esphome {
namespace light {

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

  TEMPLATABLE_VALUE(bool, state)
  TEMPLATABLE_VALUE(uint32_t, transition_length)
  TEMPLATABLE_VALUE(uint32_t, flash_length)
  TEMPLATABLE_VALUE(float, brightness)
  TEMPLATABLE_VALUE(float, red)
  TEMPLATABLE_VALUE(float, green)
  TEMPLATABLE_VALUE(float, blue)
  TEMPLATABLE_VALUE(float, white)
  TEMPLATABLE_VALUE(float, color_temperature)
  TEMPLATABLE_VALUE(std::string, effect)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();
    call.set_state(this->state_.optional_value(x...));
    call.set_brightness(this->brightness_.optional_value(x...));
    call.set_red(this->red_.optional_value(x...));
    call.set_green(this->green_.optional_value(x...));
    call.set_blue(this->blue_.optional_value(x...));
    call.set_white(this->white_.optional_value(x...));
    call.set_color_temperature(this->color_temperature_.optional_value(x...));
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
    float new_brightness = clamp(cur + rel, 0.0f, 1.0f);
    call.set_state(new_brightness != 0.0f);
    call.set_brightness(new_brightness);

    call.set_transition_length(this->transition_length_.optional_value(x...));
    call.perform();
  }

 protected:
  LightState *parent_;
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

template<typename... Ts> class AddressableSet : public Action<Ts...> {
 public:
  explicit AddressableSet(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int32_t, range_from)
  TEMPLATABLE_VALUE(int32_t, range_to)
  TEMPLATABLE_VALUE(uint8_t, red)
  TEMPLATABLE_VALUE(uint8_t, green)
  TEMPLATABLE_VALUE(uint8_t, blue)
  TEMPLATABLE_VALUE(uint8_t, white)

  void play(Ts... x) override {
    auto *out = (AddressableLight *) this->parent_->get_output();
    int32_t range_from = this->range_from_.value_or(x..., 0);
    int32_t range_to = this->range_to_.value_or(x..., out->size() - 1) + 1;
    auto range = out->range(range_from, range_to);
    if (this->red_.has_value())
      range.set_red(this->red_.value(x...));
    if (this->green_.has_value())
      range.set_green(this->green_.value(x...));
    if (this->blue_.has_value())
      range.set_blue(this->blue_.value(x...));
    if (this->white_.has_value())
      range.set_white(this->white_.value(x...));
    out->schedule_show();
  }

 protected:
  LightState *parent_;
};

}  // namespace light
}  // namespace esphome
