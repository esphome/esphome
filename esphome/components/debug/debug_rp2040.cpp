#include "debug_component.h"
#ifdef USE_RP2040
#include "esphome/core/log.h"
#include <Arduino.h>

#ifdef USE_WIFI
#include <pico/cyw43_arch.h>
#endif

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

#if defined(USE_TEXT_SENSOR) && defined(USE_WIFI)
/// @brief Helper function to convert RP2040 CYW43 WiFi power mode to string
/// @param pm WiFi power mode from cyw43_state.pm
/// @return const char pointer to the readable power mode
///
/// Maps RP2040 CYW43 WiFi power modes to user-friendly strings:
/// - CYW43_PERFORMANCE_PM (no power saving) -> "NONE"
/// - CYW43_DEFAULT_PM (default power saving) -> "LIGHT"
/// - CYW43_AGGRESSIVE_PM (aggressive power saving) -> "HIGH"
static const char *wifi_pm_to_string(uint32_t pm) {
  switch (pm) {
    case CYW43_PERFORMANCE_PM:
      return "NONE";
    case CYW43_DEFAULT_PM:
      return "LIGHT";
    case CYW43_AGGRESSIVE_PM:
      return "HIGH";
    default:
      return "UNKNOWN";
  }
}
#endif  // USE_TEXT_SENSOR && USE_WIFI

std::string DebugComponent::get_reset_reason_() { return ""; }

uint32_t DebugComponent::get_free_heap_() { return rp2040.getFreeHeap(); }

void DebugComponent::get_device_info_(std::string &device_info) {
  ESP_LOGD(TAG, "CPU Frequency: %u", rp2040.f_cpu());
  device_info += "CPU Frequency: " + to_string(rp2040.f_cpu());
}

void DebugComponent::update_platform_() {
#if defined(USE_TEXT_SENSOR) && defined(USE_WIFI)
  if (this->wifi_power_save_ != nullptr) {
    uint32_t pm = cyw43_state.pm;
    // Publish if the state has changed or if this is the first read
    if (this->last_wifi_pm_ != pm || !this->wifi_power_save_->has_state()) {
      this->wifi_power_save_->publish_state(wifi_pm_to_string(pm));
      this->last_wifi_pm_ = pm;
    }
  }
#endif
}

}  // namespace debug
}  // namespace esphome
#endif
