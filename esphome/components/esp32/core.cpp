#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "preferences.h"
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void setup();  // NOLINT(readability-redundant-declaration)

// Weak stub for initArduino - overridden when the Arduino component is present
extern "C" __attribute__((weak)) void initArduino() {}

namespace esphome {

// HAL functions live in hal.cpp. This file keeps only the loop task setup.
// Force the static TCB and stack into internal DRAM. Otherwise the linker may
// place them in PSRAM under configs like CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY,
// which is unsafe when CONFIG_SPIRAM_XIP_FROM_PSRAM is enabled because the running
// task's stack must be reachable while the flash cache is disabled.
TaskHandle_t loop_task_handle = nullptr;        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t DRAM_ATTR loop_task_tcb{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static StackType_t DRAM_ATTR
    loop_task_stack[ESPHOME_LOOP_TASK_STACK_SIZE]{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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
