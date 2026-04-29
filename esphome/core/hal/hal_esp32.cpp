#ifdef USE_ESP32

#include "esphome/core/hal.h"

#include <esp_clk_tree.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

uint32_t arch_get_cpu_freq_hz() {
  uint32_t freq = 0;
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq);
  return freq;
}

}  // namespace esphome

#endif  // USE_ESP32
