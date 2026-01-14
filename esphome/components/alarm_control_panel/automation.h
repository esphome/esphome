
#pragma once
#include "esphome/core/automation.h"
#include "alarm_control_panel.h"

namespace esphome::alarm_control_panel {

/// Trigger on any state change
class StateTrigger : public Trigger<> {
 public:
  explicit StateTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_state_callback([this]() { this->trigger(); });
  }
};

/// Template trigger that fires when entering a specific state
template<AlarmControlPanelState State> class StateEnterTrigger : public Trigger<> {
 public:
  explicit StateEnterTrigger(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {
    alarm_control_panel->add_on_state_callback([this]() {
      if (this->alarm_control_panel_->get_state() == State)
        this->trigger();
    });
  }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

// Type aliases for state-specific triggers
using TriggeredTrigger = StateEnterTrigger<ACP_STATE_TRIGGERED>;
using ArmingTrigger = StateEnterTrigger<ACP_STATE_ARMING>;
using PendingTrigger = StateEnterTrigger<ACP_STATE_PENDING>;
using ArmedHomeTrigger = StateEnterTrigger<ACP_STATE_ARMED_HOME>;
using ArmedNightTrigger = StateEnterTrigger<ACP_STATE_ARMED_NIGHT>;
using ArmedAwayTrigger = StateEnterTrigger<ACP_STATE_ARMED_AWAY>;
using DisarmedTrigger = StateEnterTrigger<ACP_STATE_DISARMED>;

/// Trigger when leaving TRIGGERED state (alarm cleared)
class ClearedTrigger : public Trigger<> {
 public:
  explicit ClearedTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_cleared_callback([this]() { this->trigger(); });
  }
};

/// Trigger on chime event (zone opened while disarmed)
class ChimeTrigger : public Trigger<> {
 public:
  explicit ChimeTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_chime_callback([this]() { this->trigger(); });
  }
};

/// Trigger on ready state change
class ReadyTrigger : public Trigger<> {
 public:
  explicit ReadyTrigger(AlarmControlPanel *alarm_control_panel) {
    alarm_control_panel->add_on_ready_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class ArmAwayAction : public Action<Ts...> {
 public:
  explicit ArmAwayAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(const Ts &...x) override {
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

  void play(const Ts &...x) override {
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

template<typename... Ts> class ArmNightAction : public Action<Ts...> {
 public:
  explicit ArmNightAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(const Ts &...x) override {
    auto call = this->alarm_control_panel_->make_call();
    auto code = this->code_.optional_value(x...);
    if (code.has_value()) {
      call.set_code(code.value());
    }
    call.arm_night();
    call.perform();
  }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class DisarmAction : public Action<Ts...> {
 public:
  explicit DisarmAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(const Ts &...x) override { this->alarm_control_panel_->disarm(this->code_.optional_value(x...)); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class PendingAction : public Action<Ts...> {
 public:
  explicit PendingAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  void play(const Ts &...x) override { this->alarm_control_panel_->make_call().pending().perform(); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class TriggeredAction : public Action<Ts...> {
 public:
  explicit TriggeredAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  void play(const Ts &...x) override { this->alarm_control_panel_->make_call().triggered().perform(); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class AlarmControlPanelCondition : public Condition<Ts...> {
 public:
  AlarmControlPanelCondition(AlarmControlPanel *parent) : parent_(parent) {}
  bool check(const Ts &...x) override {
    return this->parent_->is_state_armed(this->parent_->get_state()) ||
           this->parent_->get_state() == ACP_STATE_PENDING || this->parent_->get_state() == ACP_STATE_TRIGGERED;
  }

 protected:
  AlarmControlPanel *parent_;
};

}  // namespace esphome::alarm_control_panel
