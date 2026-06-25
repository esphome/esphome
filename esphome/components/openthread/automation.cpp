#include "esphome/core/defines.h"

#ifdef USE_OPENTHREAD

#include "automation.h"
#include "esphome/core/log.h"

namespace esphome::openthread {

static const char *const TAG = "openthread.automation";

void OpenThreadComponentBaseAction::warn_ftd_no_op_() {
  ESP_LOGW(TAG, "OpenThread action has no effect on FTD devices (MTD only)");
}

void OpenThreadComponentBaseAction::lock_and_apply_() {
  if (this->parent_->is_ready()) {
    if (auto lock = InstanceLock::try_acquire(LOCK_ACQUIRE_TIMEOUT_MS); lock) {
      if (auto *instance = lock.get_instance(); instance != nullptr) {
        this->apply_locked(instance);
      }
    } else {
      ESP_LOGW(TAG, "Failed to acquire lock in action");
    }
  } else {
    // Action may trigger early before setup, e.g. due to enabled "restore mode".
    // Trying to acquire lock would fail!
    //
    // But default component values already have been overwritten.
    // It is sufficient to let component apply those later during setup.
    ESP_LOGD(TAG, "Not (yet) ready to apply");
  }
}

}  // namespace esphome::openthread

#endif
