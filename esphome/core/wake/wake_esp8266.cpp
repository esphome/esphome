#include "esphome/core/defines.h"

#ifdef USE_ESP8266

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

namespace esphome {

// === Wake-requested flag + main-loop woke flag storage ===
// ESP8266 is always ESPHOME_THREAD_SINGLE.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;
volatile bool g_main_loop_woke = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void IRAM_ATTR wake_loop_any_context() { wake_loop_impl(); }

}  // namespace esphome

#endif  // USE_ESP8266
