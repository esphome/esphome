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

#endif  // USE_ESP8266

// === RP2040 — g_main_loop_woke definition (wake functions + wakeable_delay are inline in wake.h) ===
#ifdef USE_RP2040

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool g_main_loop_woke = false;

#endif  // USE_RP2040

// === Host (UDP loopback socket) ===
#ifdef USE_SOCKET_SELECT_SUPPORT
#include "esphome/core/application.h"
#include <sys/socket.h>

void wake_loop_threadsafe() {
  // Wakes up select() in main loop by writing to connected loopback socket
  if (App.wake_socket_fd_ >= 0) {
    const char dummy = 1;
    ::send(App.wake_socket_fd_, &dummy, 1, 0);
  }
}
#endif  // USE_SOCKET_SELECT_SUPPORT

}  // namespace esphome
