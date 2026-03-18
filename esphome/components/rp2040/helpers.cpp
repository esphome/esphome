#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#ifdef USE_RP2040

#include "esphome/core/hal.h"

#if defined(USE_WIFI)
#include <WiFi.h>
#include <pico/cyw43_arch.h>  // For cyw43_arch_lwip_begin/end (LwIPLock)
#elif defined(USE_ETHERNET)
#include <LwipEthernet.h>  // For ethernet_arch_lwip_begin/end (LwIPLock)
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

// On RP2040, lwip callbacks run from a low-priority user IRQ context, not the
// main loop (see low_priority_irq_handler() in pico-sdk
// async_context_threadsafe_background.c). This applies to both WiFi (CYW43) and
// Ethernet (W5500) — both use async_context_threadsafe_background.
//
// Without locking, recv_fn() from IRQ context races with read_locked_() on the
// main loop, corrupting the shared rx_buf_ pbuf chain (use-after-free, pbuf_cat
// assertion failures). See esphome#10681.
//
// WiFi uses cyw43_arch_lwip_begin/end; Ethernet uses ethernet_arch_lwip_begin/end.
// Both acquire the async_context recursive mutex to prevent IRQ callbacks from
// firing during critical sections.
//
// When neither WiFi nor Ethernet is configured, this is a no-op since
// there's no network stack and no lwip callbacks to race with.
#if defined(USE_WIFI)
LwIPLock::LwIPLock() { cyw43_arch_lwip_begin(); }
LwIPLock::~LwIPLock() { cyw43_arch_lwip_end(); }
#elif defined(USE_ETHERNET)
LwIPLock::LwIPLock() { ethernet_arch_lwip_begin(); }
LwIPLock::~LwIPLock() { ethernet_arch_lwip_end(); }
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
