#include "esphome/core/defines.h"

#if !defined(USE_ESP32) && !defined(USE_LIBRETINY) && !defined(USE_ESP8266) && !defined(USE_RP2040) && \
    !defined(USE_HOST)

#include "esphome/core/wake.h"

namespace esphome {

// === Wake-requested flag storage ===
// Fallback platforms (currently only Zephyr/NRF52) are ESPHOME_THREAD_SINGLE in
// real builds, but defines.h (used for static analysis / IDE) can define
// ESPHOME_THREAD_MULTI_ATOMICS here too, so the storage type has to match
// wake.h's conditional.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
std::atomic<uint8_t> g_wake_requested{0};
#else
volatile uint8_t g_wake_requested = 0;
#endif
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // fallback guard
