#include "esphome/core/wake.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP8266
#include <coredecls.h>
#endif

#ifdef USE_HOST
#include "esphome/core/application.h"
#include <sys/socket.h>
#endif

namespace esphome {

// === ESP32 — IRAM_ATTR entry points ===
#ifdef USE_ESP32
void IRAM_ATTR wake_loop_isrsafe(BaseType_t *px_higher_priority_task_woken) {
  esphome_main_task_notify_from_isr(px_higher_priority_task_woken);
}
void IRAM_ATTR wake_loop_any_context() { esphome_main_task_notify_any_context(); }
#endif

// === ESP8266 / RP2040 ===
#if defined(USE_ESP8266) || defined(USE_RP2040)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool g_main_loop_woke = false;
#endif

#ifdef USE_ESP8266
void IRAM_ATTR wake_loop_any_context() { wake_loop_impl(); }
#endif

// === RP2040 — wakeable_delay (needs file-scope state for alarm callback) ===
#ifdef USE_RP2040
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
  if (ms == 0) {
    yield();
    return;
  }
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
  while (!g_main_loop_woke && !s_delay_expired) {
    __wfe();
  }
  if (!s_delay_expired)
    cancel_alarm(alarm);
  g_main_loop_woke = false;
}
}  // namespace internal
#endif  // USE_RP2040

// === Host (UDP loopback socket) ===
#ifdef USE_HOST
void wake_loop_threadsafe() {
  if (App.wake_socket_fd_ >= 0) {
    const char dummy = 1;
    ::send(App.wake_socket_fd_, &dummy, 1, 0);
  }
}
#endif

}  // namespace esphome
