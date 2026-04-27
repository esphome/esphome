#include "esphome/core/defines.h"

#ifdef USE_RP2040

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

#include <hardware/sync.h>
#include <pico/time.h>

namespace esphome {

// === Wake-requested flag + main-loop woke flag storage ===
// RP2040 is always ESPHOME_THREAD_SINGLE.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;
volatile bool g_main_loop_woke = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile bool s_delay_expired = false;

static int64_t alarm_callback_(alarm_id_t id, void *user_data) {
  (void) id;
  (void) user_data;
  s_delay_expired = true;
  __sev();
  return 0;
}

namespace internal {
void wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  if (g_main_loop_woke) {
    g_main_loop_woke = false;
    // Yield even on the already-woken fast path so callers in tight loops
    // (e.g. lwIP raw TCP wait_for_data_) make forward progress when async
    // wakes keep re-setting g_main_loop_woke between iterations.
    yield();
    return;
  }
  s_delay_expired = false;
  alarm_id_t alarm = add_alarm_in_ms(ms, alarm_callback_, nullptr, true);
  if (alarm <= 0) {
    delay(ms);
    return;
  }
  while (!g_main_loop_woke && !s_delay_expired) {
    __wfe();
  }
  if (!s_delay_expired)
    cancel_alarm(alarm);
  g_main_loop_woke = false;
}
}  // namespace internal

}  // namespace esphome

#endif  // USE_RP2040
