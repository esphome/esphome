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

#if defined(USE_LN882X) && !defined(CLANG_TIDY)  // clang-tidy runs without the PlatformIO flags
// Fail the build if the -fshort-enums swap in __init__.py stops working.
enum Ln882xEnumSizeCheck { LN882X_ENUM_SIZE_CHECK };
static_assert(sizeof(Ln882xEnumSizeCheck) == 1, "LN882x must build with -fshort-enums to match the SDK");
#endif

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
  while (true) {
  }
}

// Measured on BK7231T (two units): cold power-up -> REBOOT_REASON_POWER; OTA and restart button -> SOFTWARE.
// Neither POWER nor WATCHDOG is a trustworthy signal: the BK7231T decoder falls back to POWER for values it does not
// recognise, and Beken (every watchdog feed) and LN882H (every boot) pre-record WATCHDOG, so a reset that does not
// clear that record reads as WATCHDOG. BK7231N and BK7238 use a different decoder and were not tested; public
// BK7231N logs report software restarts as "SW Reboot".
ResetCause arch_get_reset_cause() {
  switch (lt_get_reboot_reason()) {
    case REBOOT_REASON_POWER:
      return ResetCause::RESET_CAUSE_POWER_ON;
    case REBOOT_REASON_SOFTWARE:
      return ResetCause::RESET_CAUSE_SOFTWARE;
    case REBOOT_REASON_WATCHDOG:
      return ResetCause::RESET_CAUSE_WATCHDOG;
    case REBOOT_REASON_CRASH:
      return ResetCause::RESET_CAUSE_PANIC;
    // No family returns these today; a brownout or reset-pin event reads as WATCHDOG or POWER instead.
    case REBOOT_REASON_BROWNOUT:
      return ResetCause::RESET_CAUSE_BROWNOUT;
    case REBOOT_REASON_HARDWARE:
      return ResetCause::RESET_CAUSE_EXTERNAL;
    case REBOOT_REASON_SLEEP_GPIO:
    case REBOOT_REASON_SLEEP_RTC:
    case REBOOT_REASON_SLEEP_USB:
      return ResetCause::RESET_CAUSE_SLEEP_WAKE;
    default:  // includes REBOOT_REASON_UNKNOWN
      return ResetCause::RESET_CAUSE_UNKNOWN;
  }
}

}  // namespace esphome

#endif  // USE_LIBRETINY
