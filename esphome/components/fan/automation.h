#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "fan_state.h"

namespace esphome {
namespace fan {

template<typename... Ts> class TurnOnAction : public Action<Ts...> {
 public:
  explicit TurnOnAction(Fan *state) : state_(state) {}

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

  Fan *state_;
};

template<typename... Ts> class TurnOffAction : public Action<Ts...> {
 public:
  explicit TurnOffAction(Fan *state) : state_(state) {}

  void play(Ts... x) override { this->state_->turn_off().perform(); }

  Fan *state_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Fan *state) : state_(state) {}

  void play(Ts... x) override { this->state_->toggle().perform(); }

  Fan *state_;
};

template<typename... Ts> class CycleSpeedAction : public Action<Ts...> {
 public:
  explicit CycleSpeedAction(Fan *state) : state_(state) {}

  TEMPLATABLE_VALUE(bool, no_off_cycle)

  void play(Ts... x) override {
    // check to see if fan supports speeds and is on
    if (this->state_->get_traits().supported_speed_count()) {
      if (this->state_->state) {
        int speed = this->state_->speed + 1;
        int supported_speed_count = this->state_->get_traits().supported_speed_count();
        bool off_speed_cycle = no_off_cycle_.value(x...);
        if (speed > supported_speed_count && off_speed_cycle) {
          // was running at max speed, off speed cycle enabled, so turn off
          speed = 1;
          auto call = this->state_->turn_off();
          call.set_speed(speed);
          call.perform();
        } else if (speed > supported_speed_count && !off_speed_cycle) {
          // was running at max speed, off speed cycle disabled, so set to lowest speed
          auto call = this->state_->turn_on();
          call.set_speed(1);
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

  Fan *state_;
};

template<typename... Ts> class FanIsOnCondition : public Condition<Ts...> {
 public:
  explicit FanIsOnCondition(Fan *state) : state_(state) {}
  bool check(Ts... x) override { return this->state_->state; }

 protected:
  Fan *state_;
};
template<typename... Ts> class FanIsOffCondition : public Condition<Ts...> {
 public:
  explicit FanIsOffCondition(Fan *state) : state_(state) {}
  bool check(Ts... x) override { return !this->state_->state; }

 protected:
  Fan *state_;
};

class FanStateTrigger : public Trigger<Fan *> {
 public:
  FanStateTrigger(Fan *state) {
    state->add_on_state_callback([this, state]() { this->trigger(state); });
  }
};

class FanTurnOnTrigger : public Trigger<> {
 public:
  FanTurnOnTrigger(Fan *state) {
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
  FanTurnOffTrigger(Fan *state) {
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

class FanDirectionSetTrigger : public Trigger<FanDirection> {
 public:
  FanDirectionSetTrigger(Fan *state) {
    state->add_on_state_callback([this, state]() {
      auto direction = state->direction;
      auto should_trigger = direction != this->last_direction_;
      this->last_direction_ = direction;
      if (should_trigger) {
        this->trigger(direction);
      }
    });
    this->last_direction_ = state->direction;
  }

 protected:
  FanDirection last_direction_;
};

class FanOscillatingSetTrigger : public Trigger<bool> {
 public:
  FanOscillatingSetTrigger(Fan *state) {
    state->add_on_state_callback([this, state]() {
      auto oscillating = state->oscillating;
      auto should_trigger = oscillating != this->last_oscillating_;
      this->last_oscillating_ = oscillating;
      if (should_trigger) {
        this->trigger(oscillating);
      }
    });
    this->last_oscillating_ = state->oscillating;
  }

 protected:
  bool last_oscillating_;
};

class FanSpeedSetTrigger : public Trigger<int> {
 public:
  FanSpeedSetTrigger(Fan *state) {
    state->add_on_state_callback([this, state]() {
      auto speed = state->speed;
      auto should_trigger = speed != this->last_speed_;
      this->last_speed_ = speed;
      if (should_trigger) {
        this->trigger(speed);
      }
    });
    this->last_speed_ = state->speed;
  }

 protected:
  int last_speed_;
};

class FanPresetSetTrigger : public Trigger<std::string> {
 public:
  FanPresetSetTrigger(Fan *state) {
    state->add_on_state_callback([this, state]() {
      auto preset_mode = state->preset_mode;
      auto should_trigger = preset_mode != this->last_preset_mode_;
      this->last_preset_mode_ = preset_mode;
      if (should_trigger) {
        this->trigger(preset_mode);
      }
    });
    this->last_preset_mode_ = state->preset_mode;
  }

 protected:
  std::string last_preset_mode_;
};

}  // namespace fan
}  // namespace esphome
