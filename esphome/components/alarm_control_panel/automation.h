
#pragma once
#include "esphome/core/automation.h"
#include "alarm_control_panel.h"

namespace esphome::alarm_control_panel {

/// Callback forwarder that triggers an Automation<> on any state change.
/// Pointer-sized (single Automation* field) to fit inline in Callback::ctx_.
struct StateAnyForwarder {
  Automation<> *automation;
  void operator()(AlarmControlPanelState /*state*/) const { this->automation->trigger(); }
};

/// Callback forwarder that triggers an Automation<> only when the alarm enters a specific state.
/// Pointer-sized (single Automation* field) to fit inline in Callback::ctx_.
template<AlarmControlPanelState State> struct StateEnterForwarder {
  Automation<> *automation;
  void operator()(AlarmControlPanelState state) const {
    if (state == State)
      this->automation->trigger();
  }
};

static_assert(sizeof(StateAnyForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<StateAnyForwarder>);
static_assert(sizeof(StateEnterForwarder<ACP_STATE_TRIGGERED>) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<StateEnterForwarder<ACP_STATE_TRIGGERED>>);

template<typename... Ts> class ArmAwayAction : public Action<Ts...> {
 public:
  explicit ArmAwayAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(const Ts &...x) override { this->alarm_control_panel_->arm_away(this->code_.optional_value(x...)); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class ArmHomeAction : public Action<Ts...> {
 public:
  explicit ArmHomeAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(const Ts &...x) override { this->alarm_control_panel_->arm_home(this->code_.optional_value(x...)); }

 protected:
  AlarmControlPanel *alarm_control_panel_;
};

template<typename... Ts> class ArmNightAction : public Action<Ts...> {
 public:
  explicit ArmNightAction(AlarmControlPanel *alarm_control_panel) : alarm_control_panel_(alarm_control_panel) {}

  TEMPLATABLE_VALUE(std::string, code)

  void play(const Ts &...x) override { this->alarm_control_panel_->arm_night(this->code_.optional_value(x...)); }

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
