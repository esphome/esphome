#include "debug_component.h"
#ifdef USE_RP2040
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <Arduino.h>
#include <hardware/watchdog.h>
#if defined(PICO_RP2350)
#include <hardware/structs/powman.h>
#else
#include <hardware/structs/vreg_and_chip_reset.h>
#endif
#ifdef USE_RP2040_CRASH_HANDLER
#include "esphome/components/rp2040/crash_handler.h"
#endif
namespace esphome::debug {

static const char *const TAG = "debug";

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  char *buf = buffer.data();
  const size_t size = RESET_REASON_BUFFER_SIZE;
  size_t pos = 0;

#if defined(PICO_RP2350)
  uint32_t chip_reset = powman_hw->chip_reset;
  if (chip_reset & 0x04000000)  // HAD_GLITCH_DETECT
    pos = buf_append_str(buf, size, pos, "Power supply glitch|");
  if (chip_reset & 0x00040000)  // HAD_RUN_LOW
    pos = buf_append_str(buf, size, pos, "RUN pin|");
  if (chip_reset & 0x00020000)  // HAD_BOR
    pos = buf_append_str(buf, size, pos, "Brown-out|");
  if (chip_reset & 0x00010000)  // HAD_POR
    pos = buf_append_str(buf, size, pos, "Power-on reset|");
#else
  uint32_t chip_reset = vreg_and_chip_reset_hw->chip_reset;
  if (chip_reset & 0x00010000)  // HAD_RUN
    pos = buf_append_str(buf, size, pos, "RUN pin|");
  if (chip_reset & 0x00000100)  // HAD_POR
    pos = buf_append_str(buf, size, pos, "Power-on reset|");
#endif

  if (watchdog_caused_reboot()) {
    bool handled = false;
#ifdef USE_RP2040_CRASH_HANDLER
    if (rp2040::crash_handler_has_data()) {
      pos = buf_append_str(buf, size, pos, "Crash (HardFault)|");
      handled = true;
    }
#endif
    if (!handled) {
      if (watchdog_enable_caused_reboot()) {
        pos = buf_append_str(buf, size, pos, "Watchdog timeout|");
      } else {
        pos = buf_append_str(buf, size, pos, "Software reset|");
      }
    }
  }

  // Remove trailing '|'
  if (pos > 0 && buf[pos - 1] == '|') {
    buf[pos - 1] = '\0';
  } else if (pos == 0) {
    return "Unknown";
  }

  return buf;
}

const char *DebugComponent::get_wakeup_cause_(std::span<char, WAKEUP_CAUSE_BUFFER_SIZE> buffer) { return ""; }

uint32_t DebugComponent::get_free_heap_() { return ::rp2040.getFreeHeap(); }

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

  uint32_t cpu_freq = ::rp2040.f_cpu();
  ESP_LOGD(TAG, "CPU Frequency: %" PRIu32, cpu_freq);
  pos = buf_append_printf(buf, size, pos, "|CPU Frequency: %" PRIu32, cpu_freq);

  return pos;
}

void DebugComponent::update_platform_() {}

}  // namespace esphome::debug
#endif
