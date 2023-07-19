
#pragma once
#include "esphome/core/automation.h"
#include "alarm_control_panel.h"

namespace esphome {
namespace alarm_control_panel {

class StateTrigger : public Trigger<> {
 public:
  explicit StateTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_state_callback([this]() { this->trigger(); });
  }
};

class TriggeredTrigger : public Trigger<> {
 public:
  explicit TriggeredTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_triggered_callback([this]() { this->trigger(); });
  }
};

class ClearedTrigger : public Trigger<> {
 public:
  explicit ClearedTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_cleared_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class ArmAwayAction : public Action<Ts...> {
 public:
  explicit ArmAwayAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(Ts... x) override {
    auto call = this->alarm_control_panel_->make_call();
    auto code = this->code_.optional_value(x...);
    if (code.has_value()) {
      call.set_code(code.value());
    }
    call.arm_away();
    call.perform();
  }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class ArmHomeAction : public Action<Ts...> {
 public:
  explicit ArmHomeAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(Ts... x) override {
    auto call = this->alarm_control_panel_->make_call();
    auto code = this->code_.optional_value(x...);
    if (code.has_value()) {
      call.set_code(code.value());
    }
    call.arm_home();
    call.perform();
  }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class DisarmAction : public Action<Ts...> {
 public:
  explicit DisarmAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(Ts... x) override { this->alarm_control_panel_->disarm(this->code_.optional_value(x...)); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class PendingAction : public Action<Ts...> {
 public:
  explicit PendingAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  void play(Ts... x) override { this->alarm_control_panel_->make_call().pending().perform(); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class TriggeredAction : public Action<Ts...> {
 public:
  explicit TriggeredAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  void play(Ts... x) override { this->alarm_control_panel_->make_call().triggered().perform(); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class AlarmControlPanelCondition : public Condition<Ts...> {
 public:
  AlarmControlPanelCondition(AlarmControlPanel *parent) : parent_(parent) {}
  bool check(Ts... x) override {
    return this->parent_->is_state_armed(this->parent_->get_state()) ||
           this->parent_->get_state() == ACP_STATE_PENDING || this->parent_->get_state() == ACP_STATE_TRIGGERED;
  }

 protected:
  AlarmControlPanel *parent_;
};

}  // namespace alarm_control_panel
}  // namespace esphome
