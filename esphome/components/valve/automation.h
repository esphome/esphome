#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "valve.h"

namespace esphome {
namespace valve {

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_open().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class CloseAction : public Action<Ts...> {
 public:
  explicit CloseAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_close().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_stop().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Valve *valve) : valve_(valve) {}

  void play(const Ts &...x) override { this->valve_->make_call().set_command_toggle().perform(); }

 protected:
  Valve *valve_;
};

// All configured fields are baked into a single stateless lambda whose
// constants live in flash. The action only stores one function pointer
// plus one parent pointer, regardless of how many fields the user set.
// Trigger args are forwarded to the apply function so user lambdas
// (e.g. `position: !lambda "return x;"`) keep working.
template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(ValveCall &, const Ts &...);
  ControlAction(Valve *valve, ApplyFn apply) : valve_(valve), apply_(apply) {}

  void play(const Ts &...x) override {
    auto call = this->valve_->make_call();
    this->apply_(call, x...);
    call.perform();
  }

 protected:
  Valve *valve_;
  ApplyFn apply_;
};

template<typename... Ts> class ValveIsOpenCondition : public Condition<Ts...> {
 public:
  ValveIsOpenCondition(Valve *valve) : valve_(valve) {}
  bool check(const Ts &...x) override { return this->valve_->is_fully_open(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ValveIsClosedCondition : public Condition<Ts...> {
 public:
  ValveIsClosedCondition(Valve *valve) : valve_(valve) {}
  bool check(const Ts &...x) override { return this->valve_->is_fully_closed(); }

 protected:
  Valve *valve_;
};

class ValveOpenTrigger : public Trigger<> {
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

class ValveClosedTrigger : public Trigger<> {
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

}  // namespace valve
}  // namespace esphome
