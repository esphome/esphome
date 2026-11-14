#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#ifdef USE_RP2

#include "esphome/core/hal.h"

#if defined(USE_WIFI)
#include <WiFi.h>
#include <pico/cyw43_arch.h>  // For cyw43_arch_lwip_begin/end (LwIPMutex)
#endif
#if defined(USE_ETHERNET)
#include <lwip_wrap.h>  // For LWIPMutex — LwIPMutex mirrors its semantics (see below)
#include "esphome/components/ethernet/ethernet_component.h"
#endif
#include <hardware/structs/rosc.h>
#include <hardware/sync.h>

namespace esphome {

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
// assertion failures). See esphome#10681. The scheduler takes the same lock, as
// web_server, captive portal and web OTA call defer() from those callbacks.
//
// WiFi uses cyw43_arch_lwip_begin/end.
//
// For wired Ethernet, taking only the async_context lock is NOT enough. The
// W5500 GPIO IRQ path (LwipIntfDev::_irq) checks arduino-pico's `__inLWIP`
// counter to decide whether to defer packet processing. If we hold the
// async_context lock without bumping `__inLWIP`, an interrupt-driven packet
// arrival re-enters lwIP from IRQ context and corrupts pbufs (the `pbuf_cat`
// assertion crash on wiznet-w5500-evb-pico). LwIPMutex mirrors arduino-pico's
// LWIPMutex (cores/rp2040/lwip_wrap.h) exactly: bump `__inLWIP`, take the
// lock, and on release re-unmask any GPIO IRQs that were deferred while we
// held it. We can't `using LwIPLock = LWIPMutex;` in helpers.h because
// pulling lwip_wrap.h there poisons many TUs with lwIP types.
//
// Ethernet comes first so Wi-Fi + Ethernet also bumps `__inLWIP`;
// ethernet_arch_lwip_begin() takes cyw43_arch_lwip_begin() itself on a Pico W.
//
// When neither WiFi nor Ethernet is configured, this is a no-op since
// there's no network stack and no lwip callbacks to race with.
#if defined(USE_ETHERNET)
void LwIPMutex::lock() {
  __inLWIP = __inLWIP + 1;
  ethernet_arch_lwip_begin();
}
void LwIPMutex::unlock() {
  ethernet_arch_lwip_end();
  __inLWIP = __inLWIP - 1;
  if (__needsIRQEN && !__inLWIP) {
    __needsIRQEN = false;
    ethernet_arch_lwip_gpio_unmask();
  }
}
#elif defined(USE_WIFI)
// No async context when CYW43 init was skipped (a Pico W image on a plain Pico): nothing to lock out
void LwIPMutex::lock() {
  if (cyw43_arch_async_context() != nullptr)
    cyw43_arch_lwip_begin();
}
void LwIPMutex::unlock() {
  if (cyw43_arch_async_context() != nullptr)
    cyw43_arch_lwip_end();
}
#else
void LwIPMutex::lock() {}
void LwIPMutex::unlock() {}
#endif

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
#ifdef USE_WIFI
  WiFi.macAddress(mac);
#elif defined(USE_ETHERNET)
  ethernet::global_eth_component->get_eth_mac_address_raw(mac);
#endif
}

}  // namespace esphome

#endif  // USE_RP2
