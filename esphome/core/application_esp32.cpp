#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {

static const char *const TAG = "app_esp32";

void ICACHE_RAM_ATTR HOT Application::feed_wdt_arch_() {
#if CONFIG_ARDUINO_RUNNING_CORE == 0
#ifdef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0
  // ESP32 uses "Task Watchdog" which is hooked to the FreeRTOS idle task.
  // To cause the Watchdog to be triggered we need to put the current task
  // to sleep to get the idle task scheduled.
  delay(1);
#endif
#endif
}

}  // namespace esphome
#endif
