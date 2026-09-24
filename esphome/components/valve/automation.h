#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "valve.h"

namespace esphome::valve {

template<typename... Ts> class OpenAction final : public Action<Ts...> {
 public:
  explicit OpenAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_open().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class CloseAction final : public Action<Ts...> {
 public:
  explicit CloseAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_close().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class StopAction final : public Action<Ts...> {
 public:
  explicit StopAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_stop().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ToggleAction final : public Action<Ts...> {
 public:
  explicit ToggleAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_toggle().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ValveIsOpenCondition final : public Condition<Ts...> {
 public:
  ValveIsOpenCondition(Valve *valve) : valve_(valve) {}
  bool check(const Ts &...x) override { return this->valve_->is_fully_open(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ValveIsClosedCondition final : public Condition<Ts...> {
 public:
  ValveIsClosedCondition(Valve *valve) : valve_(valve) {}
  bool check(const Ts &...x) override { return this->valve_->is_fully_closed(); }

 protected:
  Valve *valve_;
};

class ValveOpenTrigger final : public Trigger<> {
 public:
  ValveOpenTrigger(Valve *a_valve) : valve_(a_valve) {
    a_valve->add_on_state_callback([this]() {
      if (this->valve_->is_fully_open()) {
        this->trigger();
      }
    });
  }

 protected:
  Valve *valve_;
};

class ValveClosedTrigger final : public Trigger<> {
 public:
  ValveClosedTrigger(Valve *a_valve) : valve_(a_valve) {
    a_valve->add_on_state_callback([this]() {
      if (this->valve_->is_fully_closed()) {
        this->trigger();
      }
    });
  }

 protected:
  Valve *valve_;
};

}  // namespace esphome::valve
