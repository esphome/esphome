#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#ifdef USE_RP2040

#include "esphome/core/hal.h"

#if defined(USE_WIFI)
#include <WiFi.h>
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

// RP2040 doesn't have mutexes, but that shouldn't be an issue as it's single-core and non-preemptive OS.
Mutex::Mutex() {}
Mutex::~Mutex() {}
void Mutex::lock() {}
bool Mutex::try_lock() { return true; }
void Mutex::unlock() {}

IRAM_ATTR InterruptLock::InterruptLock() { state_ = save_and_disable_interrupts(); }
IRAM_ATTR InterruptLock::~InterruptLock() { restore_interrupts(state_); }

// RP2040 doesn't support lwIP core locking, so this is a no-op
LwIPLock::LwIPLock() {}
LwIPLock::~LwIPLock() {}

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
#ifdef USE_WIFI
  WiFi.macAddress(mac);
#endif
}

}  // namespace esphome

#endif  // USE_RP2040
