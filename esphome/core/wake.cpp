#include "esphome/core/wake.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP8266
#include <coredecls.h>
#elif defined(USE_RP2040)
#include <hardware/sync.h>
#include <pico/time.h>
#endif

namespace esphome {

// === ESP32 — IRAM_ATTR entry points (inline impls in wake.h) ===
#ifdef USE_ESP32

void IRAM_ATTR wake_loop_isrsafe(int *px_higher_priority_task_woken) {
  wake_loop_isrsafe_inline_(px_higher_priority_task_woken);
}

void IRAM_ATTR wake_loop_any_context() { wake_loop_any_context_inline_(); }

#endif  // USE_ESP32

// === ESP8266 — IRAM_ATTR entry point + wakeable_delay ===
#ifdef USE_ESP8266

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool g_main_loop_woke = false;

void IRAM_ATTR wake_loop_any_context() { wake_loop_impl_(); }

void wakeable_delay(uint32_t ms) {
  if (ms == 0) {
    delay(0);
    return;
  }
  g_main_loop_woke = false;
  esp_delay(ms, []() { return !g_main_loop_woke; });
}

#endif  // USE_ESP8266

// === RP2040 — wakeable_delay (wake functions are inline in wake.h) ===
#ifdef USE_RP2040

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool g_main_loop_woke = false;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile bool s_delay_expired = false;

static int64_t alarm_callback_(alarm_id_t id, void *user_data) {
  (void) id;
  (void) user_data;
  s_delay_expired = true;
  __sev();   // Wake from __wfe() — timeout expired.
  return 0;  // One-shot
}

void wakeable_delay(uint32_t ms) {
  if (ms == 0) {
    yield();
    return;
  }
  // If a wake was already signalled, consume it and return immediately
  if (g_main_loop_woke) {
    g_main_loop_woke = false;
    return;
  }
  s_delay_expired = false;
  alarm_id_t alarm = add_alarm_in_ms(ms, alarm_callback_, nullptr, true);
  if (alarm <= 0) {
    delay(ms);
    return;
  }
  // Sleep until woken by either the timer alarm or wake_loop_any_context()/wake_loop_threadsafe()
  while (!g_main_loop_woke && !s_delay_expired) {
    __wfe();
  }
  if (!s_delay_expired)
    cancel_alarm(alarm);
  g_main_loop_woke = false;
}

#endif  // USE_RP2040

// === Host (UDP loopback socket) ===
#ifdef USE_SOCKET_SELECT_SUPPORT
#include "esphome/core/application.h"
#include <lwip/sockets.h>

void wake_loop_threadsafe() {
  // Wakes up lwip_select() in main loop by writing to connected loopback socket
  if (App.wake_socket_fd_ >= 0) {
    const char dummy = 1;
    lwip_send(App.wake_socket_fd_, &dummy, 1, 0);
  }
}
#endif  // USE_SOCKET_SELECT_SUPPORT

}  // namespace esphome
