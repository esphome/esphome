#include "esphome/core/helpers.h"

#ifdef USE_LIBRETINY

#include "esphome/core/hal.h"

#include <WiFi.h>  // for macAddress()

namespace esphome {

uint32_t random_uint32() { return rand(); }

bool random_bytes(uint8_t *data, size_t len) {
  lt_rand_bytes(data, len);
  return true;
}

// only affects the executing core
// so should not be used as a mutex lock, only to get accurate timing
IRAM_ATTR InterruptLock::InterruptLock() { portDISABLE_INTERRUPTS(); }
IRAM_ATTR InterruptLock::~InterruptLock() { portENABLE_INTERRUPTS(); }

// LibreTiny LwIPLock is defined inline as a no-op in helpers.h

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  WiFi.macAddress(mac);
}

}  // namespace esphome

#endif  // USE_LIBRETINY
