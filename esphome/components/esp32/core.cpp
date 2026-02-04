#ifdef USE_ESP32

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"
#include <esp_clk_tree.h>
#include <esp_cpu.h>
#include <esp_idf_version.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void setup();  // NOLINT(readability-redundant-declaration)
void loop();   // NOLINT(readability-redundant-declaration)

// Weak stub for initArduino - overridden when the Arduino component is present
extern "C" __attribute__((weak)) void initArduino() {}

namespace esphome {

void IRAM_ATTR HOT yield() { vPortYield(); }
uint32_t IRAM_ATTR HOT millis() { return (uint32_t) (esp_timer_get_time() / 1000ULL); }
void IRAM_ATTR HOT delay(uint32_t ms) { vTaskDelay(ms / portTICK_PERIOD_MS); }
uint32_t IRAM_ATTR HOT micros() { return (uint32_t) esp_timer_get_time(); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  esp_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
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
void IRAM_ATTR HOT arch_feed_wdt() { esp_task_wdt_reset(); }

uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
uint32_t arch_get_cpu_cycle_count() { return esp_cpu_get_cycle_count(); }
uint32_t arch_get_cpu_freq_hz() {
  uint32_t freq = 0;
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq);
  return freq;
}

TaskHandle_t loop_task_handle = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void loop_task(void *pv_params) {
  setup();
  while (true) {
    loop();
  }
}

extern "C" void app_main() {
  initArduino();
  esp32::setup_preferences();
#if CONFIG_FREERTOS_UNICORE
  xTaskCreate(loop_task, "loopTask", ESPHOME_LOOP_TASK_STACK_SIZE, nullptr, 1, &loop_task_handle);
#else
  xTaskCreatePinnedToCore(loop_task, "loopTask", ESPHOME_LOOP_TASK_STACK_SIZE, nullptr, 1, &loop_task_handle, 1);
#endif
}

}  // namespace esphome

#endif  // USE_ESP32
