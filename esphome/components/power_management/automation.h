
#include "power_management.h"

namespace esphome {
namespace power_management {

template<typename... Ts> class AcquireLockAction : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  void play(Ts... x) override {
    this->parent_->acquire_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::CPU);
  }
};

template<typename... Ts> class ReleaseLockAction : public Action<Ts...>, public Parented<PowerManagement> {
 public:
  void play(Ts... x) override {
    this->parent_->release_lock(PowerManagementLockUser::ACTION, PowerManagementLockType::CPU);
  }
};

}  // namespace power_management
}  // namespace esphome
