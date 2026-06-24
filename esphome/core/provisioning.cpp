#include "esphome/core/provisioning.h"
#ifdef USE_PROVISIONING
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {

static const char *const TAG = "provisioning";

ProvisioningManager *global_provisioning_manager =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;

ProvisioningManager::ProvisioningManager() { global_provisioning_manager = this; }

void ProvisioningManager::loop() {
  // Nothing to do once provisioned or already closed.
  if (this->closed_ || this->is_provisioned())
    return;
  // The window timer runs from boot (millis since boot). The closed state is not
  // persisted, so a reboot reopens the window.
  if (this->timeout_ != 0 && App.get_loop_component_start_time() > this->timeout_) {
    this->close_window_();
  }
}

void ProvisioningManager::close_window_() {
  this->closed_ = true;
  ESP_LOGW(TAG, "Window expired; cycle power to reopen window");
  // Notify internal consumers first (transports disconnect clients, Improv stops),
  // then fire the user-facing automation.
  this->closed_callback_.call();
  this->timeout_trigger_.trigger();
}

void ProvisioningManager::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Provisioning:\n"
                "  Timeout: %" PRIu32 "ms\n"
                "  Provisioned: %s",
                this->timeout_, YESNO(this->is_provisioned()));
}

}  // namespace esphome
#endif  // USE_PROVISIONING
