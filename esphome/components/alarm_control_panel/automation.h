
#pragma once
#include "esphome/core/automation.h"
#include "alarm_control_panel.h"

namespace esphome {
namespace alarm_control_panel {

/// Trigger on any state change
class StateTrigger final : public Trigger<>, public AlarmControlPanelStateListener {
 public:
  explicit StateTrigger(AlarmControlPanel *alarm_control_panel) { alarm_control_panel->add_listener(this); }
  void on_state(AlarmControlPanelState new_state, AlarmControlPanelState prev_state) override { this->trigger(); }
};

/// Template trigger that fires when entering a specific state
template<AlarmControlPanelState State>
class StateEnterTrigger final : public Trigger<>, public AlarmControlPanelStateListener {
 public:
  explicit StateEnterTrigger(AlarmControlPanel *alarm_control_panel) { alarm_control_panel->add_listener(this); }
  void on_state(AlarmControlPanelState new_state, AlarmControlPanelState prev_state) override {
    if (new_state == State)
      this->trigger();
  }
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
class ClearedTrigger final : public Trigger<>, public AlarmControlPanelStateListener {
 public:
  explicit ClearedTrigger(AlarmControlPanel *alarm_control_panel) { alarm_control_panel->add_listener(this); }
  void on_state(AlarmControlPanelState new_state, AlarmControlPanelState prev_state) override {
    if (prev_state == ACP_STATE_TRIGGERED)
      this->trigger();
  }
};

/// Trigger on chime event (zone opened while disarmed)
class ChimeTrigger final : public Trigger<>, public AlarmControlPanelEventListener {
 public:
  explicit ChimeTrigger(AlarmControlPanel *alarm_control_panel) { alarm_control_panel->add_listener(this); }
  void on_chime() override { this->trigger(); }
};

/// Trigger on ready state change
class ReadyTrigger final : public Trigger<>, public AlarmControlPanelEventListener {
 public:
  explicit ReadyTrigger(AlarmControlPanel *alarm_control_panel) { alarm_control_panel->add_listener(this); }
  void on_ready() override { this->trigger(); }
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

}  // namespace alarm_control_panel
}  // namespace esphome
