#pragma once

#include <type_traits>

#include "esphome/core/automation.h"
#include "pd.h"

namespace esphome::fusb302b {

/// Triggers when a source is attached, including after recovering from a communication error.
struct ConnectForwarder {
  Automation<> *automation;
  void operator()(PdState state, PdState previous) const {
    if (is_connected_state(state) && !is_connected_state(previous))
      this->automation->trigger();
  }
};

/// Triggers when the source is removed, or is no longer known to be attached because communication failed.
struct DisconnectForwarder {
  Automation<> *automation;
  void operator()(PdState state, PdState previous) const {
    if (!is_connected_state(state) && is_connected_state(previous))
      this->automation->trigger();
  }
};

/// Triggers when negotiation is finished: a new PD contract is in place, or the source does not speak PD.
struct PowerReadyForwarder {
  Automation<> *automation;
  void operator()(PdState state, PdState /*previous*/) const {
    if (state == PdState::PD_STATE_EXPLICIT_CONTRACT || state == PdState::PD_STATE_PD_TIMEOUT)
      this->automation->trigger();
  }
};

/// Triggers when communication with the chip fails.
struct ErrorForwarder {
  Automation<> *automation;
  void operator()(PdState state, PdState previous) const {
    if (state == PdState::PD_STATE_ERROR && previous != PdState::PD_STATE_ERROR)
      this->automation->trigger();
  }
};

static_assert(sizeof(ConnectForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<ConnectForwarder>);
static_assert(sizeof(DisconnectForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<DisconnectForwarder>);
static_assert(sizeof(PowerReadyForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<PowerReadyForwarder>);
static_assert(sizeof(ErrorForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<ErrorForwarder>);

}  // namespace esphome::fusb302b
