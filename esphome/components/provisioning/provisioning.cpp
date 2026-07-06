#include "esphome/components/provisioning/provisioning.h"
#ifdef USE_PROVISIONING
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::provisioning {

static const char *const TAG = "provisioning";

ProvisioningManager *global_provisioning_manager =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;

ProvisioningManager::ProvisioningManager() { global_provisioning_manager = this; }

uint8_t ProvisioningManager::register_source() {
  if (this->source_count_ >= MAX_SOURCES) {
    // Defensive: only a handful of sources exist in practice. Fail loudly rather
    // than shifting past the mask width (undefined behavior). The returned index is
    // ignored by set_source_provisioned()'s bounds check.
    ESP_LOGE(TAG, "Too many provisioning sources (max %u)", MAX_SOURCES);
    return this->source_count_;
  }
  uint8_t source = this->source_count_++;
  this->registered_mask_ |= (1UL << source);
  return source;
}

void ProvisioningManager::loop() {
  // The window is resolved once the device is provisioned or the window has closed;
  // there is nothing left to track, so stop running entirely.
  //
  // The registered_mask_ != 0 guard is essential: sources register during their own
  // setup(), which runs after ours (we are BEFORE_CONNECTION). ESPHome also runs
  // already-setup components' loop() while waiting on a slow component during setup,
  // so this loop() can run before any source has registered. Without the guard,
  // is_provisioned() would be vacuously true (no registered sources) and we would
  // disable_loop() permanently before the window ever starts.
  if (this->closed_ || (this->registered_mask_ != 0 && this->is_provisioned())) {
    this->disable_loop();
    return;
  }
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
  if (this->registered_mask_ == 0) {
    // No transport registered as a source, so the window never opens and nothing
    // can close it. Warn rather than silently leaving the option inert.
    ESP_LOGW(TAG, "No provisioning-capable component configured (e.g. 'api:' with "
                  "'encryption:' and no 'key'); the provisioning window is inert");
  }
}

}  // namespace esphome::provisioning
#endif  // USE_PROVISIONING
