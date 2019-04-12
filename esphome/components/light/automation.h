#pragma once

#include "esphome/core/automation.h"
#include "light_state.h"

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
    this->play_next(x...);
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
    this->play_next(x...);
  }

 protected:
  LightState *parent_;
};

template<typename... Ts> class LightIsOnCondition : public Condition<Ts...> {
 public:
  explicit LightIsOnCondition(LightState *state) : state_(state) {}
  bool check(Ts... x) override { return this->state_->get_current_values().is_on(); }

 protected:
  LightState *state_;
};
template<typename... Ts> class LightIsOffCondition : public Condition<LightState, Ts...> {
 public:
  explicit LightIsOffCondition(LightState *state) : state_(state) {}
  bool check(Ts... x) override { return !this->state_->get_current_values().is_on(); }

 protected:
  LightState *state_;
};

}  // namespace light
}  // namespace esphome
