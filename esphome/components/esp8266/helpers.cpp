#include "esphome/core/helpers.h"

#ifdef USE_ESP8266

#include <osapi.h>
#include <user_interface.h>
// for xt_rsil()/xt_wsr_ps()
#include <Arduino.h>

namespace esphome {

uint32_t random_uint32() { return os_random(); }
bool random_bytes(uint8_t *data, size_t len) { return os_get_random(data, len) == 0; }

// ESP8266 Mutex is defined inline as a no-op in helpers.h when USE_ESP8266 (or USE_RP2040) is set,
// independent of the ESPHOME_THREAD_SINGLE thread model define.

IRAM_ATTR InterruptLock::InterruptLock() { state_ = xt_rsil(15); }
IRAM_ATTR InterruptLock::~InterruptLock() { xt_wsr_ps(state_); }

// ESP8266 LwIPLock is defined inline as a no-op in helpers.h

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  wifi_get_macaddr(STATION_IF, mac);
}

}  // namespace esphome

#endif  // USE_ESP8266
