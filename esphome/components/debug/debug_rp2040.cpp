#include "debug_component.h"
#ifdef USE_RP2040
#include "esphome/core/log.h"
#include <Arduino.h>
#include <hardware/watchdog.h>
#if defined(PICO_RP2350)
#include <hardware/structs/powman.h>
#else
#include <hardware/structs/vreg_and_chip_reset.h>
#endif
namespace esphome {
namespace debug {

static const char *const TAG = "debug";

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  char *buf = buffer.data();
  const size_t size = RESET_REASON_BUFFER_SIZE;
  size_t pos = 0;

#if defined(PICO_RP2350)
  uint32_t chip_reset = powman_hw->chip_reset;
  if (chip_reset & 0x10000000)  // HAD_WATCHDOG_RESET_RSM
    pos = buf_append_str(buf, size, pos, "Watchdog (RSM)|");
  if (chip_reset & 0x08000000)  // HAD_HZD_SYS_RESET_REQ
    pos = buf_append_str(buf, size, pos, "Hazard debugger reset|");
  if (chip_reset & 0x04000000)  // HAD_GLITCH_DETECT
    pos = buf_append_str(buf, size, pos, "Power supply glitch|");
  if (chip_reset & 0x02000000)  // HAD_SWCORE_PD
    pos = buf_append_str(buf, size, pos, "Switched core powerdown|");
  if (chip_reset & 0x01000000)  // HAD_WATCHDOG_RESET_SWCORE
    pos = buf_append_str(buf, size, pos, "Watchdog (SWCORE)|");
  if (chip_reset & 0x00800000)  // HAD_WATCHDOG_RESET_POWMAN
    pos = buf_append_str(buf, size, pos, "Watchdog (POWMAN)|");
  if (chip_reset & 0x00400000)  // HAD_WATCHDOG_RESET_POWMAN_ASYNC
    pos = buf_append_str(buf, size, pos, "Watchdog (POWMAN async)|");
  if (chip_reset & 0x00200000)  // HAD_RESCUE
    pos = buf_append_str(buf, size, pos, "Rescue reset|");
  if (chip_reset & 0x00080000)  // HAD_DP_RESET_REQ
    pos = buf_append_str(buf, size, pos, "Debugger reset|");
  if (chip_reset & 0x00040000)  // HAD_RUN_LOW
    pos = buf_append_str(buf, size, pos, "RUN pin|");
  if (chip_reset & 0x00020000)  // HAD_BOR
    pos = buf_append_str(buf, size, pos, "Brown-out|");
  if (chip_reset & 0x00010000)  // HAD_POR
    pos = buf_append_str(buf, size, pos, "Power-on reset|");
#else
  uint32_t chip_reset = vreg_and_chip_reset_hw->chip_reset;
  if (chip_reset & 0x00100000)  // HAD_PSM_RESTART
    pos = buf_append_str(buf, size, pos, "Debug port restart|");
  if (chip_reset & 0x00010000)  // HAD_RUN
    pos = buf_append_str(buf, size, pos, "RUN pin|");
  if (chip_reset & 0x00000100)  // HAD_POR
    pos = buf_append_str(buf, size, pos, "Power-on reset|");
#endif

  if (watchdog_caused_reboot()) {
    if (watchdog_enable_caused_reboot()) {
      pos = buf_append_str(buf, size, pos, "Watchdog timeout|");
    } else {
      pos = buf_append_str(buf, size, pos, "Watchdog reboot|");
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

const char *DebugComponent::get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) { return ""; }

uint32_t DebugComponent::get_free_heap_() { return rp2040.getFreeHeap(); }

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

  uint32_t cpu_freq = rp2040.f_cpu();
  ESP_LOGD(TAG, "CPU Frequency: %" PRIu32, cpu_freq);
  pos = buf_append_printf(buf, size, pos, "|CPU Frequency: %" PRIu32, cpu_freq);

  return pos;
}

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif
