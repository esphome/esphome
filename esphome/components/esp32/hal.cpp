#ifdef USE_ESP32

// defines.h must come before crash_handler.h so USE_ESP32_CRASH_HANDLER is set
// before crash_handler.h's #ifdef-guarded namespace block is parsed.
#include "esphome/core/defines.h"
#include "crash_handler.h"
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

void arch_init() {
#ifdef USE_ESP32_CRASH_HANDLER
  // Read crash data from previous boot before anything else
  esp32::crash_handler_read_and_clear();
#endif

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
