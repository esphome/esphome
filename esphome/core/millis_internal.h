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
    // 1 kHz: xTaskGetTickCount() is already ms.
    static_assert(configTICK_RATE_HZ == 1000, "MillisInternal fast path requires 1 kHz FreeRTOS tick");
    return xTaskGetTickCount();
#elif defined(USE_BK72XX)
    // 500 Hz: scale by portTICK_PERIOD_MS (== 2). Inlined to avoid the
    // out-of-line call to esphome::millis() (IRAM_ATTR is a no-op on BK72xx —
    // SDK masks FIQ + IRQ during flash writes, see hal.h).
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
