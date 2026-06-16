#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "fan.h"

namespace esphome::fan {

// All configured fields are baked into a single stateless lambda whose
// constants live in flash. The action only stores one function pointer
// plus one parent pointer, regardless of how many fields the user set.
// Trigger args are forwarded to the apply function so user lambdas
// (e.g. `speed: !lambda "return x;"`) keep working.
//
// Trigger args are normalized to `const std::remove_cvref_t<Ts> &...` so
// the codegen can emit a matching parameter list for both the apply lambda
// and any inner field lambdas without producing invalid C++ source text
// (e.g. `const T & &` if Ts already carries a reference, or `const const
// T &` if Ts already carries a const). This keeps trigger args no-copy
// regardless of whether the trigger supplies `T`, `T &`, or `const T &`.
template<typename... Ts> class TurnOnAction : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(FanCall &, const std::remove_cvref_t<Ts> &...);
  TurnOnAction(Fan *state, ApplyFn apply) : state_(state), apply_(apply) {}

  void play(const Ts &...x) override {
    auto call = this->state_->turn_on();
    this->apply_(call, x...);
    call.perform();
  }

  Fan *state_;
  ApplyFn apply_;
};

template<typename... Ts> class TurnOffAction : public Action<Ts...> {
 public:
  explicit TurnOffAction(Fan *state) : state_(state) {}

  void play(const Ts &...x) override { this->state_->turn_off().perform(); }

  Fan *state_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Fan *state) : state_(state) {}

  void play(const Ts &...x) override { this->state_->toggle().perform(); }

  Fan *state_;
};

template<typename... Ts> class CycleSpeedAction : public Action<Ts...> {
 public:
  explicit CycleSpeedAction(Fan *state) : state_(state) {}

  TEMPLATABLE_VALUE(bool, no_off_cycle)

  void play(const Ts &...x) override {
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
  bool check(const Ts &...x) override { return this->state_->state; }

 protected:
  Fan *state_;
};
template<typename... Ts> class FanIsOffCondition : public Condition<Ts...> {
 public:
  explicit FanIsOffCondition(Fan *state) : state_(state) {}
  bool check(const Ts &...x) override { return !this->state_->state; }

 protected:
  Fan *state_;
};

class FanStateTrigger : public Trigger<Fan *> {
 public:
  FanStateTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() { this->trigger(this->fan_); });
  }

 protected:
  Fan *fan_;
};

class FanTurnOnTrigger : public Trigger<> {
 public:
  FanTurnOnTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() {
      auto is_on = this->fan_->state;
      auto should_trigger = is_on && !this->last_on_;
      this->last_on_ = is_on;
      if (should_trigger) {
        this->trigger();
      }
    });
    this->last_on_ = state->state;
  }

 protected:
  Fan *fan_;
  bool last_on_;
};

class FanTurnOffTrigger : public Trigger<> {
 public:
  FanTurnOffTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() {
      auto is_on = this->fan_->state;
      auto should_trigger = !is_on && this->last_on_;
      this->last_on_ = is_on;
      if (should_trigger) {
        this->trigger();
      }
    });
    this->last_on_ = state->state;
  }

 protected:
  Fan *fan_;
  bool last_on_;
};

class FanDirectionSetTrigger : public Trigger<FanDirection> {
 public:
  FanDirectionSetTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() {
      auto direction = this->fan_->direction;
      auto should_trigger = direction != this->last_direction_;
      this->last_direction_ = direction;
      if (should_trigger) {
        this->trigger(direction);
      }
    });
    this->last_direction_ = state->direction;
  }

 protected:
  Fan *fan_;
  FanDirection last_direction_;
};

class FanOscillatingSetTrigger : public Trigger<bool> {
 public:
  FanOscillatingSetTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() {
      auto oscillating = this->fan_->oscillating;
      auto should_trigger = oscillating != this->last_oscillating_;
      this->last_oscillating_ = oscillating;
      if (should_trigger) {
        this->trigger(oscillating);
      }
    });
    this->last_oscillating_ = state->oscillating;
  }

 protected:
  Fan *fan_;
  bool last_oscillating_;
};

class FanSpeedSetTrigger : public Trigger<int> {
 public:
  FanSpeedSetTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() {
      auto speed = this->fan_->speed;
      auto should_trigger = speed != this->last_speed_;
      this->last_speed_ = speed;
      if (should_trigger) {
        this->trigger(speed);
      }
    });
    this->last_speed_ = state->speed;
  }

 protected:
  Fan *fan_;
  int last_speed_;
};

class FanPresetSetTrigger : public Trigger<StringRef> {
 public:
  FanPresetSetTrigger(Fan *state) : fan_(state) {
    state->add_on_state_callback([this]() {
      auto preset_mode = this->fan_->get_preset_mode();
      auto should_trigger = preset_mode != this->last_preset_mode_;
      this->last_preset_mode_ = preset_mode;
      if (should_trigger) {
        this->trigger(preset_mode);
      }
    });
    this->last_preset_mode_ = state->get_preset_mode();
  }

 protected:
  Fan *fan_;
  StringRef last_preset_mode_{};
};

}  // namespace esphome::fan
