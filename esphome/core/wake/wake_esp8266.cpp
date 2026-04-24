#include "esphome/core/defines.h"

#ifdef USE_ESP8266

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

namespace esphome {

// === Wake-requested flag + main-loop woke flag storage ===
// ESP8266 is always ESPHOME_THREAD_SINGLE in real builds, but defines.h (used
// for static analysis / IDE) can define ESPHOME_THREAD_MULTI_ATOMICS alongside
// USE_ESP8266, so the storage type here has to match wake.h's conditional.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
std::atomic<uint8_t> g_wake_requested{0};
#else
volatile uint8_t g_wake_requested = 0;
#endif
volatile bool g_main_loop_woke = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void IRAM_ATTR wake_loop_any_context() { wake_loop_impl(); }

}  // namespace esphome

#endif  // USE_ESP8266
