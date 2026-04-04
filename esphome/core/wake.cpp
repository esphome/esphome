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
void IRAM_ATTR wake_loop_isrsafe(int *px_higher_priority_task_woken) {
  wake_loop_isrsafe_inline_(px_higher_priority_task_woken);
}
void IRAM_ATTR wake_loop_any_context() { wake_loop_any_context_inline_(); }
#endif

// === ESP8266 / RP2040 ===
#if defined(USE_ESP8266) || defined(USE_RP2040)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool g_main_loop_woke = false;
#endif

#ifdef USE_ESP8266
void IRAM_ATTR wake_loop_any_context() { wake_loop_impl_(); }
#endif

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
