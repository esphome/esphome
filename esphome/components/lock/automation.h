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

template<LockState State> class LockStateTrigger : public Trigger<> {
 public:
  explicit LockStateTrigger(Lock *a_lock) {
    a_lock->add_on_state_callback([this, a_lock]() {
      if (a_lock->state == State) {
        this->trigger();
      }
    });
  }
};

using LockLockTrigger = LockStateTrigger<LockState::LOCK_STATE_LOCKED>;
using LockUnlockTrigger = LockStateTrigger<LockState::LOCK_STATE_UNLOCKED>;

}  // namespace esphome::lock
