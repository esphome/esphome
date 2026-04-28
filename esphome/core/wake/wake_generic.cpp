#include "esphome/core/defines.h"

#if !defined(USE_ESP32) && !defined(USE_LIBRETINY) && !defined(USE_ESP8266) && !defined(USE_RP2040) && \
    !defined(USE_HOST)

#include "esphome/core/wake.h"

namespace esphome {

// === Wake-requested flag storage ===
// Fallback platforms (currently only Zephyr/NRF52) are ESPHOME_THREAD_SINGLE.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;

}  // namespace esphome

#endif  // fallback guard
