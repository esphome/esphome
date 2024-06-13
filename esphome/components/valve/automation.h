#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "valve.h"

namespace esphome {
namespace valve {

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Valve *valve) : valve_(valve) {}

  void play(Ts... x) override { this->valve_->make_call().set_command_open().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class CloseAction : public Action<Ts...> {
 public:
  explicit CloseAction(Valve *valve) : valve_(valve) {}

  void play(Ts... x) override { this->valve_->make_call().set_command_close().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(Valve *valve) : valve_(valve) {}

  void play(Ts... x) override { this->valve_->make_call().set_command_stop().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Valve *valve) : valve_(valve) {}

  void play(Ts... x) override { this->valve_->make_call().set_command_toggle().perform(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Valve *valve) : valve_(valve) {}

  TEMPLATABLE_VALUE(bool, stop)
  TEMPLATABLE_VALUE(float, position)

  void play(Ts... x) override {
    auto call = this->valve_->make_call();
    if (this->stop_.has_value())
      call.set_stop(this->stop_.value(x...));
    if (this->position_.has_value())
      call.set_position(this->position_.value(x...));
    call.perform();
  }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ValvePublishAction : public Action<Ts...> {
 public:
  ValvePublishAction(Valve *valve) : valve_(valve) {}
  TEMPLATABLE_VALUE(float, position)
  TEMPLATABLE_VALUE(ValveOperation, current_operation)

  void play(Ts... x) override {
    if (this->position_.has_value())
      this->valve_->position = this->position_.value(x...);
    if (this->current_operation_.has_value())
      this->valve_->current_operation = this->current_operation_.value(x...);
    this->valve_->publish_state();
  }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ValveIsOpenCondition : public Condition<Ts...> {
 public:
  ValveIsOpenCondition(Valve *valve) : valve_(valve) {}
  bool check(Ts... x) override { return this->valve_->is_fully_open(); }

 protected:
  Valve *valve_;
};

template<typename... Ts> class ValveIsClosedCondition : public Condition<Ts...> {
 public:
  ValveIsClosedCondition(Valve *valve) : valve_(valve) {}
  bool check(Ts... x) override { return this->valve_->is_fully_closed(); }

 protected:
  Valve *valve_;
};

class ValveOpenTrigger : public Trigger<> {
 public:
  ValveOpenTrigger(Valve *a_valve) {
    a_valve->add_on_state_callback([this, a_valve]() {
      if (a_valve->is_fully_open()) {
        this->trigger();
      }
    });
  }
};

class ValveClosedTrigger : public Trigger<> {
 public:
  ValveClosedTrigger(Valve *a_valve) {
    a_valve->add_on_state_callback([this, a_valve]() {
      if (a_valve->is_fully_closed()) {
        this->trigger();
      }
    });
  }
};

}  // namespace valve
}  // namespace esphome
