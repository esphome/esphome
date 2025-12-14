#pragma once

#include "power_management.h"

namespace esphome {
namespace power_management {

template<typename... Ts> class AcquireLockAction : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  TEMPLATABLE_VALUE(std::string, lock_type)
  TEMPLATABLE_VALUE(uint32_t, timer_lock_duration)
  void play(Ts... x) override {
    if (this->lock_type_.value(x...) == "TMR") {
      if (this->timer_lock_duration_.value(x...) > 0) {
        this->parent_->set_timer_lock_duration(this->timer_lock_duration_.value(x...));
      }
      this->parent_->acquire_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::TMR);
    } else if (this->lock_type_.value(x...) == "CPU") {
      this->parent_->acquire_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::CPU);
    } else if (this->lock_type_.value(x...) == "APB") {
      this->parent_->acquire_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::APB);
    } else if (this->lock_type_.value(x...) == "SLP") {
      this->parent_->acquire_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::SLP);
    }
  }
};

template<typename... Ts> class ReleaseLockAction : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  TEMPLATABLE_VALUE(std::string, lock_type)
  void play(Ts... x) override {
    if (this->lock_type_.value(x...) == "CPU") {
      this->parent_->release_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::CPU);
    } else if (this->lock_type_.value(x...) == "APB") {
      this->parent_->release_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::APB);
    } else if (this->lock_type_.value(x...) == "SLP") {
      this->parent_->release_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::SLP);
    }
  }
};

}  // namespace power_management
}  // namespace esphome
