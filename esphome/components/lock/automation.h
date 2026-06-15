#pragma once

#include "esphome/components/lock/lock.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::lock {

template<typename... Ts> class LockAction : public Action<Ts...> {
 public:
  explicit LockAction(Lock *a_lock) : lock_(a_lock) {}

  void play(const Ts &...x) override { this->lock_->lock(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class UnlockAction : public Action<Ts...> {
 public:
  explicit UnlockAction(Lock *a_lock) : lock_(a_lock) {}

  void play(const Ts &...x) override { this->lock_->unlock(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Lock *a_lock) : lock_(a_lock) {}

  void play(const Ts &...x) override { this->lock_->open(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class LockCondition : public Condition<Ts...> {
 public:
  LockCondition(Lock *parent, bool state) : parent_(parent), state_(state) {}
  bool check(const Ts &...x) override {
    auto check_state = this->state_ ? LockState::LOCK_STATE_LOCKED : LockState::LOCK_STATE_UNLOCKED;
    return this->parent_->state == check_state;
  }

 protected:
  Lock *parent_;
  bool state_;
};

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
