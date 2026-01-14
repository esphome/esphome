#include "debug_component.h"
#ifdef USE_RP2040
#include "esphome/core/log.h"
#include <Arduino.h>
namespace esphome {
namespace debug {

static const char *const TAG = "debug";

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) { return ""; }

const char *DebugComponent::get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) { return ""; }

uint32_t DebugComponent::get_free_heap_() { return rp2040.getFreeHeap(); }

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

  uint32_t cpu_freq = rp2040.f_cpu();
  ESP_LOGD(TAG, "CPU Frequency: %" PRIu32, cpu_freq);
  pos = buf_append(buf, size, pos, "|CPU Frequency: %" PRIu32, cpu_freq);

  return pos;
}

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif
