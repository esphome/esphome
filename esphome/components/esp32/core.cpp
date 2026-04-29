#ifdef USE_ESP32

#include "esphome/core/defines.h"
#include "crash_handler.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void setup();  // NOLINT(readability-redundant-declaration)

// Weak stub for initArduino - overridden when the Arduino component is present
extern "C" __attribute__((weak)) void initArduino() {}

namespace esphome {

// yield(), delay(), micros(), millis_64() inlined in hal.h.
// millis(), arch_restart(), arch_get_cpu_freq_hz() out-of-line in hal/hal_esp32.cpp.
// delayMicroseconds(), arch_feed_wdt(), arch_get_cpu_cycle_count() inlined in hal/hal_esp32.h.
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
