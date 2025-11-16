#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace switch_ {

template<typename... Ts> class TurnOnAction : public Action<Ts...> {
 public:
  explicit TurnOnAction(Switch *a_switch) : switch_(a_switch) {}

  void play(const Ts &...x) override { this->switch_->turn_on(); }

 protected:
  Switch *switch_;
};

template<typename... Ts> class TurnOffAction : public Action<Ts...> {
 public:
  explicit TurnOffAction(Switch *a_switch) : switch_(a_switch) {}

  void play(const Ts &...x) override { this->switch_->turn_off(); }

 protected:
  Switch *switch_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Switch *a_switch) : switch_(a_switch) {}

  void play(const Ts &...x) override { this->switch_->toggle(); }

 protected:
  Switch *switch_;
};

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Switch *a_switch) : switch_(a_switch) {}

  TEMPLATABLE_VALUE(bool, state)

  void play(const Ts &...x) override {
    auto state = this->state_.optional_value(x...);
    if (state.has_value()) {
      this->switch_->control(*state);
    }
  }

 protected:
  Switch *switch_;
};

template<typename... Ts> class SwitchCondition : public Condition<Ts...> {
 public:
  SwitchCondition(Switch *parent, bool state) : parent_(parent), state_(state) {}
  bool check(const Ts &...x) override { return this->parent_->state == this->state_; }

 protected:
  Switch *parent_;
  bool state_;
};

class SwitchStateTrigger : public Trigger<bool> {
 public:
  SwitchStateTrigger(Switch *a_switch) {
    a_switch->add_on_state_callback([this](bool state) { this->trigger(state); });
  }
};

class SwitchTurnOnTrigger : public Trigger<> {
 public:
  SwitchTurnOnTrigger(Switch *a_switch) {
    a_switch->add_on_state_callback([this](bool state) {
      if (state) {
        this->trigger();
      }
    });
  }
};

class SwitchTurnOffTrigger : public Trigger<> {
 public:
  SwitchTurnOffTrigger(Switch *a_switch) {
    a_switch->add_on_state_callback([this](bool state) {
      if (!state) {
        this->trigger();
      }
    });
  }
};

template<typename... Ts> class SwitchPublishAction : public Action<Ts...> {
 public:
  SwitchPublishAction(Switch *a_switch) : switch_(a_switch) {}
  TEMPLATABLE_VALUE(bool, state)

  void play(const Ts &...x) override { this->switch_->publish_state(this->state_.value(x...)); }

 protected:
  Switch *switch_;
};

}  // namespace switch_
}  // namespace esphome
