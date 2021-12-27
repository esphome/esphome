#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/lock/lock.h"

namespace esphome {
namespace lock_ {

template<typename... Ts> class LockAction : public Action<Ts...> {
 public:
  explicit LockAction(Lock *a_lock) : lock_(a_lock) {}

  void play(Ts... x) override { this->lock_->lock(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class UnlockAction : public Action<Ts...> {
 public:
  explicit UnlockAction(Lock *a_lock) : lock_(a_lock) {}

  void play(Ts... x) override { this->lock_->unlock(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Lock *a_lock) : lock_(a_lock) {}

  void play(Ts... x) override { this->lock_->toggle(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Lock *a_lock) : lock_(a_lock) {}

  void play(Ts... x) override { this->lock_->open(); }

 protected:
  Lock *lock_;
};

template<typename... Ts> class LockCondition : public Condition<Ts...> {
 public:
  LockCondition(Lock *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->state == this->state_; }

 protected:
  Lock *parent_;
  bool state_;
};

class LockLockTrigger : public Trigger<> {
 public:
  LockLockTrigger(Lock *a_lock) {
    a_lock->add_on_state_callback([this](bool state) {
      if (state) {
        this->trigger();
      }
    });
  }
};

class LockUnlockTrigger : public Trigger<> {
 public:
  LockUnlockTrigger(Lock *a_lock) {
    a_lock->add_on_state_callback([this](bool state) {
      if (!state) {
        this->trigger();
      }
    });
  }
};

class LockOpenTrigger : public Trigger<> {
 public:
  LockOpenTrigger(Lock *a_lock) {
    a_lock->add_on_state_callback([this](bool state) {
      if (!state) {
        this->trigger();
      }
    });
  }
};

template<typename... Ts> class LockPublishAction : public Action<Ts...> {
 public:
  LockPublishAction(Lock *a_lock) : lock_(a_lock) {}
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override { this->lock_->publish_state(this->state_.value(x...)); }

 protected:
  Lock *lock_;
};

}  // namespace lock_
}  // namespace esphome
