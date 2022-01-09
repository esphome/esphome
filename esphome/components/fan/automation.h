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
  TEMPLATABLE_VALUE(int, speed)
  TEMPLATABLE_VALUE(FanDirection, direction)

  void play(Ts... x) override {
    auto call = this->state_->turn_on();
    if (this->oscillating_.has_value()) {
      call.set_oscillating(this->oscillating_.value(x...));
    }
    if (this->speed_.has_value()) {
      call.set_speed(this->speed_.value(x...));
    }
    if (this->direction_.has_value()) {
      call.set_direction(this->direction_.value(x...));
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

template<typename... Ts> class CycleSpeedAction : public Action<Ts...> {
 public:
  explicit CycleSpeedAction(FanState *state) : state_(state) {}

  void play(Ts... x) override {
    // check to see if fan supports speeds and is on
    if (this->state_->get_traits().supported_speed_count()) {
      if (this->state_->state) {
        int speed = this->state_->speed + 1;
        int supported_speed_count = this->state_->get_traits().supported_speed_count();
        if (speed > supported_speed_count) {
          // was running at max speed, so turn off
          speed = 1;
          auto call = this->state_->turn_off();
          call.set_speed(speed);
          call.perform();
        } else {
          auto call = this->state_->turn_on();
          call.set_speed(speed);
          call.perform();
        }
      } else {
        // fan was off, so set speed to 1
        auto call = this->state_->turn_on();
        call.set_speed(1);
        call.perform();
      }
    } else {
      // fan doesn't support speed counts, so toggle
      this->state_->toggle().perform();
    }
  }

  FanState *state_;
};

template<typename... Ts> class FanIsOnCondition : public Condition<Ts...> {
 public:
  explicit FanIsOnCondition(FanState *state) : state_(state) {}
  bool check(Ts... x) override { return this->state_->state; }

 protected:
  FanState *state_;
};
template<typename... Ts> class FanIsOffCondition : public Condition<Ts...> {
 public:
  explicit FanIsOffCondition(FanState *state) : state_(state) {}
  bool check(Ts... x) override { return !this->state_->state; }

 protected:
  FanState *state_;
};

class FanTurnOnTrigger : public Trigger<> {
 public:
  FanTurnOnTrigger(FanState *state) {
    state->add_on_state_callback([this, state]() {
      auto is_on = state->state;
      auto should_trigger = is_on && !this->last_on_;
      this->last_on_ = is_on;
      if (should_trigger) {
        this->trigger();
      }
    });
    this->last_on_ = state->state;
  }

 protected:
  bool last_on_;
};

class FanTurnOffTrigger : public Trigger<> {
 public:
  FanTurnOffTrigger(FanState *state) {
    state->add_on_state_callback([this, state]() {
      auto is_on = state->state;
      auto should_trigger = !is_on && this->last_on_;
      this->last_on_ = is_on;
      if (should_trigger) {
        this->trigger();
      }
    });
    this->last_on_ = state->state;
  }

 protected:
  bool last_on_;
};

class FanSpeedSetTrigger : public Trigger<> {
 public:
  FanSpeedSetTrigger(FanState *state) {
    state->add_on_state_callback([this, state]() {
      auto speed = state->speed;
      auto should_trigger = speed != !this->last_speed_;
      this->last_speed_ = speed;
      if (should_trigger) {
        this->trigger();
      }
    });
    this->last_speed_ = state->speed;
  }

 protected:
  int last_speed_;
};

}  // namespace fan
}  // namespace esphome
