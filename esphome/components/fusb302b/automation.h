#pragma once

#include <type_traits>

#include "esphome/core/automation.h"
#include "pd.h"

namespace esphome::fusb302b {

/// Triggers when a source is attached.
struct ConnectForwarder {
  Automation<> *automation;
  void operator()(PdState state, PdState previous) const {
    if (previous == PdState::PD_STATE_DISCONNECTED && state != PdState::PD_STATE_DISCONNECTED &&
        state != PdState::PD_STATE_ERROR)
      this->automation->trigger();
  }
};

/// Triggers when the source is removed.
struct DisconnectForwarder {
  Automation<> *automation;
  void operator()(PdState state, PdState previous) const {
    if (state == PdState::PD_STATE_DISCONNECTED && previous != PdState::PD_STATE_DISCONNECTED &&
        previous != PdState::PD_STATE_ERROR)
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
