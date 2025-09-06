#include "esphome/core/helpers.h"

#ifdef USE_STM32

namespace esphome {

uint32_t random_uint32() { return rand(); }
bool random_bytes(uint8_t *data, size_t len) {
  while (len--)
    *data++ = random_uint32() & 0xff;
  return true;
}

// STM32 doesn't have mutexes, but that shouldn't be an issue as it's single-core and non-preemptive OS.
Mutex::Mutex() {}
Mutex::~Mutex() {}
void Mutex::lock() {}
bool Mutex::try_lock() { return true; }
void Mutex::unlock() {}

InterruptLock::InterruptLock() {}
InterruptLock::~InterruptLock() {}

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  memset(mac, 0, 6);
}

}  // namespace esphome

#endif  // USE_STM32
