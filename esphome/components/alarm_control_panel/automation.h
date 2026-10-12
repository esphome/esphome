
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

}  // namespace esphome::alarm_control_panel
