#pragma once

#include "power_management.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace power_management {

template<typename... Ts> class AcquireLockAction : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  TEMPLATABLE_VALUE(std::string, lock_type)
  void play(Ts... x) override {
#ifdef USE_POWER_MANAGEMENT
    if (this->lock_type_.value(x...) == "CPU") {
      this->parent_->acquire_lock(PowerManagementLockType::CPU);
    } else if (this->lock_type_.value(x...) == "APB") {
      this->parent_->acquire_lock(PowerManagementLockType::APB);
    } else if (this->lock_type_.value(x...) == "SLP") {
      this->parent_->acquire_lock(PowerManagementLockType::SLP);
    }
#endif
  }
};

template<typename... Ts> class ReleaseLockAction : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  TEMPLATABLE_VALUE(std::string, lock_type)
  void play(Ts... x) override {
#ifdef USE_POWER_MANAGEMENT
    if (this->lock_type_.value(x...) == "CPU") {
      this->parent_->release_lock(PowerManagementLockType::CPU);
    } else if (this->lock_type_.value(x...) == "APB") {
      this->parent_->release_lock(PowerManagementLockType::APB);
    } else if (this->lock_type_.value(x...) == "SLP") {
      this->parent_->release_lock(PowerManagementLockType::SLP);
    }
#endif
  }
};

}  // namespace power_management
}  // namespace esphome
