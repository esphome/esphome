#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/switch/switch.h"

namespace esphome::switch_ {

template<typename... Ts> class SwitchCondition final : public Condition<Ts...> {
 public:
  SwitchCondition(Switch *parent, bool state) : parent_(parent), state_(state) {}
  bool check(const Ts &...x) override { return this->parent_->state == this->state_; }

 protected:
  Switch *parent_;
  bool state_;
};

class SwitchStateTrigger final : public Trigger<bool> {
 public:
  SwitchStateTrigger(Switch *a_switch) {
    a_switch->add_on_state_callback([this](bool state) { this->trigger(state); });
  }
};

class SwitchTurnOnTrigger final : public Trigger<> {
 public:
  SwitchTurnOnTrigger(Switch *a_switch) {
    a_switch->add_on_state_callback([this](bool state) {
      if (state) {
        this->trigger();
      }
    });
  }
};

class SwitchTurnOffTrigger final : public Trigger<> {
 public:
  SwitchTurnOffTrigger(Switch *a_switch) {
    a_switch->add_on_state_callback([this](bool state) {
      if (!state) {
        this->trigger();
      }
    });
  }
};

}  // namespace esphome::switch_
