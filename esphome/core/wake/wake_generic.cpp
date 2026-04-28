#include "esphome/core/defines.h"

#if !defined(USE_ESP32) && !defined(USE_LIBRETINY) && !defined(USE_ESP8266) && !defined(USE_RP2040) && \
    !defined(USE_HOST)

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

#ifdef USE_ZEPHYR
#include <zephyr/kernel.h>
#endif

namespace esphome {

#ifdef USE_ZEPHYR
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
K_SEM_DEFINE(esphome_wake_sem, 0, 1);
#endif

// === Wake-requested flag storage ===
// Fallback platforms (currently only Zephyr/NRF52) are ESPHOME_THREAD_SINGLE.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;

#ifdef USE_ZEPHYR
void wake_loop_threadsafe() {
  wake_request_set();
  k_sem_give(&esphome_wake_sem);
}

namespace internal {
void wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  k_sem_take(&esphome_wake_sem, ms == UINT32_MAX ? K_FOREVER : K_MSEC(ms));
}
}  // namespace internal
#endif  // USE_ZEPHYR

}  // namespace esphome

#endif  // fallback guard
