#ifdef USE_LIBRETINY

#include "core.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/time_64.h"
#include "esphome/core/helpers.h"
#include "preferences.h"

#include <FreeRTOS.h>
#include <task.h>

void setup();
void loop();

namespace esphome {

void HOT yield() { ::yield(); }
uint32_t IRAM_ATTR HOT millis() { return ::millis(); }
uint64_t millis_64() { return Millis64Impl::compute(::millis()); }
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void HOT delay(uint32_t ms) { ::delay(ms); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }

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
  // This is safe because ESPHome yields voluntarily via yield_with_select_() and
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
void HOT arch_feed_wdt() { lt_wdt_feed(); }
uint32_t arch_get_cpu_cycle_count() { return lt_cpu_get_cycle_count(); }
uint32_t arch_get_cpu_freq_hz() { return lt_cpu_get_freq(); }

}  // namespace esphome

#endif  // USE_LIBRETINY
