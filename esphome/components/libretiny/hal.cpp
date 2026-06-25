#ifdef USE_LIBRETINY

#include "core.h"
#include "esphome/core/hal.h"
#include "preferences.h"

#include <FreeRTOS.h>
#include <task.h>

// Empty libretiny namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// libretiny component's API.
namespace esphome::libretiny {}  // namespace esphome::libretiny

namespace esphome {

// yield(), delay(), micros(), millis(), millis_64(), delayMicroseconds(),
// arch_feed_wdt(), arch_get_cpu_cycle_count(), arch_get_cpu_freq_hz()
// inlined in components/libretiny/hal.h.

void arch_init() {
  libretiny::setup_preferences();
  lt_wdt_enable(10000L);
#ifdef USE_BK72XX
  // BK72xx SDK creates the main Arduino task at priority 3, which is lower than
  // all WiFi (4-5), LwIP (4), and TCP/IP (7) tasks. This causes ~100ms loop
  // stalls whenever WiFi background processing runs, because the main task
  // cannot resume until every higher-priority task finishes.
  //
  // By contrast, RTL87xx creates the main task at osPriorityRealtime (highest).
  //
  // Raise to priority 6: above WiFi/LwIP tasks (4-5) so they don't preempt the
  // main loop, but below the TCP/IP thread (7) so packet processing keeps priority.
  // This is safe because ESPHome yields voluntarily via wakeable_delay() and
  // the Arduino mainTask yield() after each loop() iteration.
  static constexpr UBaseType_t MAIN_TASK_PRIORITY = 6;
  static_assert(MAIN_TASK_PRIORITY < configMAX_PRIORITIES, "MAIN_TASK_PRIORITY must be less than configMAX_PRIORITIES");
  vTaskPrioritySet(nullptr, MAIN_TASK_PRIORITY);
#endif
#if LT_GPIO_RECOVER
  lt_gpio_recover();
#endif
}

void arch_restart() {
  lt_reboot();
  while (1) {
  }
}

}  // namespace esphome

#endif  // USE_LIBRETINY
