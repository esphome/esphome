#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#ifdef USE_RP2040

#include "esphome/core/hal.h"

#if defined(USE_WIFI)
#include <WiFi.h>
#include <pico/cyw43_arch.h>  // For cyw43_arch_lwip_begin/end (LwIPLock)
#endif
#include <hardware/structs/rosc.h>
#include <hardware/sync.h>

namespace esphome {

uint32_t random_uint32() {
  uint32_t result = 0;
  for (uint8_t i = 0; i < 32; i++) {
    result <<= 1;
    result |= rosc_hw->randombit;
  }
  return result;
}

bool random_bytes(uint8_t *data, size_t len) {
  while (len-- != 0) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
      result <<= 1;
      result |= rosc_hw->randombit;
    }
    *data++ = result;
  }
  return true;
}

// RP2040 Mutex is defined inline in helpers.h for RP2040/ESP8266 builds.

IRAM_ATTR InterruptLock::InterruptLock() { state_ = save_and_disable_interrupts(); }
IRAM_ATTR InterruptLock::~InterruptLock() { restore_interrupts(state_); }

// On RP2040 (Pico W), arduino-pico sets PICO_CYW43_ARCH_THREADSAFE_BACKGROUND=1.
// This means lwip callbacks run from a low-priority user IRQ context, not the
// main loop (see low_priority_irq_handler() in pico-sdk
// async_context_threadsafe_background.c). cyw43_arch_lwip_begin/end acquires the
// async_context recursive mutex to prevent IRQ callbacks from firing during
// critical sections. See esphome#10681.
//
// When CYW43 is not available (non-WiFi RP2040 boards), this is a no-op since
// there's no network stack and no lwip callbacks to race with.
#if defined(USE_WIFI)
LwIPLock::LwIPLock() { cyw43_arch_lwip_begin(); }
LwIPLock::~LwIPLock() { cyw43_arch_lwip_end(); }
#else
LwIPLock::LwIPLock() {}
LwIPLock::~LwIPLock() {}
#endif

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
#ifdef USE_WIFI
  WiFi.macAddress(mac);
#endif
}

}  // namespace esphome

#endif  // USE_RP2040
