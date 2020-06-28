#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "fan_state.h"

namespace esphome {
namespace fan {

template<typename... Ts> class TurnOnAction : public Action<Ts...> {
 public:
  explicit TurnOnAction(FanState *state) : state_(state) {}

  TEMPLATABLE_VALUE(bool, oscillating)
  TEMPLATABLE_VALUE(FanSpeed, speed)

  void play(Ts... x) override {
    auto call = this->state_->turn_on();
    if (this->oscillating_.has_value()) {
      call.set_oscillating(this->oscillating_.value(x...));
    }
    if (this->speed_.has_value()) {
      call.set_speed(this->speed_.value(x...));
    }
    call.perform();
  }

  FanState *state_;
};

template<typename... Ts> class TurnOffAction : public Action<Ts...> {
 public:
  explicit TurnOffAction(FanState *state) : state_(state) {}

  void play(Ts... x) override { this->state_->turn_off().perform(); }

  FanState *state_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(FanState *state) : state_(state) {}

  void play(Ts... x) override { this->state_->toggle().perform(); }

  FanState *state_;
};

}  // namespace fan
}  // namespace esphome
