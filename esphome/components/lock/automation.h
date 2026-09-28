#pragma once

#include "esphome/components/lock/lock.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::lock {

/// Callback forwarder that triggers an Automation<> only when a specific lock state is entered.
/// Pointer-sized (single Automation* field) to fit inline in Callback::ctx_.
template<LockState State> struct LockStateForwarder {
  Automation<> *automation;
  void operator()(LockState state) const {
    if (state == State)
      this->automation->trigger();
  }
};

static_assert(sizeof(LockStateForwarder<LockState::LOCK_STATE_LOCKED>) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<LockStateForwarder<LockState::LOCK_STATE_LOCKED>>);

}  // namespace esphome::lock
