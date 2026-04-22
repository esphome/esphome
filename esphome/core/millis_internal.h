#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#if defined(USE_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#elif defined(USE_LIBRETINY)
#include <FreeRTOS.h>
#include <task.h>
#endif

namespace esphome {

// Friend-gated accessor for a fast millis() variant intended only for
// known task-context callers on the main loop hot path (Application::loop()
// and WarnIfComponentBlockingGuard::finish()). It skips the ISR-context
// dispatch that the public esphome::millis() pays on ESP32 and libretiny.
//
// MUST NOT be called from ISR context: on ESP32 and libretiny it calls the
// non-FromISR FreeRTOS API directly, which is undefined behavior in ISR
// context.
//
// Adding new callers requires adding a friend declaration here — that
// is the review point. Do not relax the access (e.g. by making get()
// public) without considering the ISR-safety contract.
//
// Other platforms currently delegate to the public millis(); the friend
// gate still enforces the intent so platform-specific fast paths can be
// added later without changing call sites.
class MillisInternal {
 private:
  static ESPHOME_ALWAYS_INLINE uint32_t get() {
#if defined(USE_ESP32) && CONFIG_FREERTOS_HZ == 1000
    return xTaskGetTickCount();
#elif defined(USE_LIBRETINY) && (defined(USE_RTL87XX) || defined(USE_LN882X))
    // RTL87xx and LN882x run FreeRTOS at 1 kHz, so xTaskGetTickCount() is
    // already in milliseconds.
    static_assert(configTICK_RATE_HZ == 1000, "MillisInternal fast path requires 1 kHz FreeRTOS tick");
    return xTaskGetTickCount();
#elif defined(USE_BK72XX)
    // BK72xx runs FreeRTOS at 500 Hz; scale ticks by portTICK_PERIOD_MS (== 2).
    // Inlined here because esphome::millis() on BK72xx is out-of-line (its
    // IRAM_ATTR is a no-op — see hal.h — because the BK72xx SDK wraps flash
    // operations in GLOBAL_INT_DISABLE() which masks FIQ + IRQ at the CPU for
    // the duration of every write, so no ISR runs while flash is stalled and
    // IRAM placement / FromISR dispatch are both unnecessary). Calling the
    // out-of-line esphome::millis() would still cost a real function call,
    // which this inlined path avoids.
    static_assert(configTICK_RATE_HZ == 500, "BK72xx MillisInternal assumes 500 Hz FreeRTOS tick");
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
#else
    return millis();
#endif
  }
  friend class Application;
  friend class WarnIfComponentBlockingGuard;
};

}  // namespace esphome
