#include "debug_component.h"
#ifdef USE_RP2040
#include "esphome/core/log.h"
#include <Arduino.h>
namespace esphome {
namespace debug {

static const char *const TAG = "debug";

std::string DebugComponent::get_reset_reason_() { return ""; }

uint32_t DebugComponent::get_free_heap_() { return rp2040.getFreeHeap(); }

void DebugComponent::get_device_info_(std::string &device_info) {
  ESP_LOGD(TAG, "CPU Frequency: %u", rp2040.f_cpu());
  device_info += "CPU Frequency: " + to_string(rp2040.f_cpu());
}

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif
