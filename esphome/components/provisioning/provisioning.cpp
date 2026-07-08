#include "esphome/components/provisioning/provisioning.h"
#ifdef USE_PROVISIONING
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif

#include <cinttypes>

namespace esphome::provisioning {

static const char *const TAG = "provisioning";

ProvisioningManager *global_provisioning_manager =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;

ProvisioningManager::ProvisioningManager() {
  global_provisioning_manager = this;
#ifdef USE_NETWORK
  // Network connectivity is a built-in provisioning source. Registered here rather
  // than from a source's setup() because connectivity is universal, not a pluggable
  // transport; loop() latches it provisioned once the device has connected.
  this->network_source_ = this->register_source();
#endif
}

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
  // Sources register during their own setup() (at various priorities), and this
  // loop() also runs while waiting on a slow component during setup. Evaluating the
  // provisioning state before every source has registered could conclude
  // "provisioned" prematurely and disable_loop() for good, defeating the window --
  // so do nothing until all setup() calls are done.
  if (!App.is_setup_complete())
    return;

#ifdef USE_NETWORK
  // Latch the built-in connectivity source once the device has been reachable via
  // any interface. network::is_connected() aggregates wifi/ethernet/modem/... (OR
  // across interfaces), and a disabled interface never connects so it never
  // contributes. Latched: a later link drop does not un-provision -- the RAM-only
  // window still reopens only on reboot.
  if ((this->provisioned_mask_ & (1UL << this->network_source_)) == 0 && network::is_connected())
    this->set_source_provisioned(this->network_source_, true);
#endif

  // The window is resolved once the device is provisioned or the window has closed;
  // there is nothing left to track, so stop running entirely. Config validation
  // guarantees at least one source, so is_provisioned() is never vacuously true here.
  if (this->closed_ || this->is_provisioned()) {
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
}

}  // namespace esphome::provisioning
#endif  // USE_PROVISIONING
