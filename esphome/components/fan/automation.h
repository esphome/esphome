#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "fan.h"

namespace esphome::fan {

/// Codegen use only: step `fan` to its next speed. Past the top speed it turns off when `off_speed_cycle` is set,
/// otherwise it goes back to speed 1. A fan without speeds is toggled.
void cycle_speed(Fan *fan, bool off_speed_cycle);

class FanTurnOnTrigger final : public Trigger<> {
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

class FanTurnOffTrigger final : public Trigger<> {
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

class FanDirectionSetTrigger final : public Trigger<FanDirection> {
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

class FanOscillatingSetTrigger final : public Trigger<bool> {
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

class FanSpeedSetTrigger final : public Trigger<int> {
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

class FanPresetSetTrigger final : public Trigger<StringRef> {
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
