#ifdef USE_ESP32

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include <esp_clk_tree.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Empty esp32 namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// esp32 component's API.
namespace esphome::esp32 {}  // namespace esphome::esp32

namespace esphome {

// Use xTaskGetTickCount() when tick rate is 1 kHz (ESPHome's default via sdkconfig),
// falling back to esp_timer for non-standard rates. IRAM_ATTR is required because
// Wiegand and ZyAura call millis() from IRAM_ATTR ISR handlers on ESP32.
// xTaskGetTickCountFromISR() is used in ISR context to satisfy the FreeRTOS API contract.
uint32_t IRAM_ATTR HOT millis() {
#if CONFIG_FREERTOS_HZ == 1000
  if (xPortInIsrContext()) [[unlikely]] {
    return xTaskGetTickCountFromISR();
  }
  return xTaskGetTickCount();
#else
  return micros_to_millis(static_cast<uint64_t>(esp_timer_get_time()));
#endif
}

void arch_restart() {
  esp_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}

// Measured on ESP32-S3: cold power-up -> ESP_RST_POWERON; OTA and restart button -> ESP_RST_SW.
// Values newer than ESP-IDF 5.0 are not named, so they fall through to UNKNOWN. On some ESP32-S3 SPIRAM configurations
// esp_restart() reads as ESP_RST_WDT; debug/debug_esp32.cpp corrects that with a stored marker, but that would put a
// flash write on every reboot into core, so the docs suggest giving `watchdog` the same value as `software` instead.
ResetCause arch_get_reset_cause() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return ResetCause::RESET_CAUSE_POWER_ON;
    case ESP_RST_SW:
      return ResetCause::RESET_CAUSE_SOFTWARE;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return ResetCause::RESET_CAUSE_WATCHDOG;
    case ESP_RST_PANIC:
      return ResetCause::RESET_CAUSE_PANIC;
    case ESP_RST_BROWNOUT:
      return ResetCause::RESET_CAUSE_BROWNOUT;
    // No ESP-IDF target returns this (a reset-pin press reads as ESP_RST_POWERON); mapped for completeness.
    case ESP_RST_EXT:
      return ResetCause::RESET_CAUSE_EXTERNAL;
    case ESP_RST_DEEPSLEEP:
      return ResetCause::RESET_CAUSE_SLEEP_WAKE;
    default:
      return ResetCause::RESET_CAUSE_UNKNOWN;
  }
}

void arch_init() {
  // Enable the task watchdog only on the loop task (from which we're currently running)
  esp_task_wdt_add(nullptr);

  // Handle OTA rollback: mark partition valid immediately unless USE_OTA_ROLLBACK is enabled,
  // in which case safe_mode will mark it valid after confirming successful boot.
#ifndef USE_OTA_ROLLBACK
  esp_ota_mark_app_valid_cancel_rollback();
#endif
}

uint32_t arch_get_cpu_freq_hz() {
  uint32_t freq = 0;
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq);
  return freq;
}

}  // namespace esphome

#endif  // USE_ESP32
