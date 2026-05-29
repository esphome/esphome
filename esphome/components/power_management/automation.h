#pragma once

#include "power_management.h"
#include "esphome/core/automation.h"

namespace esphome::power_management {

template<typename... Ts> class LockActionBase : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  TEMPLATABLE_VALUE(PowerManagementLockType, lock_type)
};

template<typename... Ts> class AcquireLockAction : public LockActionBase<Ts...> {
 public:
  void play(Ts... x) override {
#ifdef USE_POWER_MANAGEMENT
    this->parent_->acquire_lock(this->lock_type_.value(x...));
#endif
  }
};

template<typename... Ts> class ReleaseLockAction : public LockActionBase<Ts...> {
 public:
  void play(Ts... x) override {
#ifdef USE_POWER_MANAGEMENT
    this->parent_->release_lock(this->lock_type_.value(x...));
#endif
  }
};

}  // namespace esphome::power_management
