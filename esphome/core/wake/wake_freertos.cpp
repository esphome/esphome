#include "esphome/core/defines.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

namespace esphome {

// === Wake-requested flag storage ===
// ESP32 is always MULTI_ATOMICS; LibreTiny is MULTI_ATOMICS on chips with
// proper atomics (e.g. RTL8720) and MULTI_NO_ATOMICS on others (e.g. BK72XX).
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<uint8_t> g_wake_requested{0};
#else
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;
#endif

void IRAM_ATTR wake_loop_isrsafe(BaseType_t *px_higher_priority_task_woken) {
  // ISR-safe: set flag before notify so the wake is visible on the next gate
  // check. wake_request_set() is just an aligned 8-bit store / atomic store
  // and is safe from IRAM.
  wake_request_set();
  esphome_main_task_notify_from_isr(px_higher_priority_task_woken);
}

void IRAM_ATTR wake_loop_any_context() { wake_main_task_any_context(); }

}  // namespace esphome

#endif  // USE_ESP32 || USE_LIBRETINY
