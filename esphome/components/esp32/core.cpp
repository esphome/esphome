#ifdef USE_ESP32

#include "esphome/core/defines.h"
#include "crash_handler.h"
#include "esphome/core/application.h"
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

// Weak stub for initArduino - overridden when the Arduino component is present
extern "C" __attribute__((weak)) void initArduino() {}

namespace esphome {

// yield(), delay(), micros(), millis_64() inlined in hal.h.
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
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
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
void HOT arch_feed_wdt() { esp_task_wdt_reset(); }

uint32_t arch_get_cpu_cycle_count() { return esp_cpu_get_cycle_count(); }
uint32_t arch_get_cpu_freq_hz() {
  uint32_t freq = 0;
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &freq);
  return freq;
}

TaskHandle_t loop_task_handle = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t loop_task_tcb;        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static StackType_t
    loop_task_stack[ESPHOME_LOOP_TASK_STACK_SIZE];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void loop_task(void *pv_params) {
  setup();
  while (true) {
    App.loop();
  }
}

extern "C" void app_main() {
  initArduino();
  esp32::setup_preferences();
#if CONFIG_FREERTOS_UNICORE
  loop_task_handle = xTaskCreateStatic(loop_task, "loopTask", ESPHOME_LOOP_TASK_STACK_SIZE, nullptr, 1, loop_task_stack,
                                       &loop_task_tcb);
#else
  loop_task_handle = xTaskCreateStaticPinnedToCore(loop_task, "loopTask", ESPHOME_LOOP_TASK_STACK_SIZE, nullptr, 1,
                                                   loop_task_stack, &loop_task_tcb, 1);
#endif
}

}  // namespace esphome

#endif  // USE_ESP32
