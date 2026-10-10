#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

namespace esphome {

// HAL functions live in hal.cpp.

Mutex::Mutex() {
  auto *mutex = new k_mutex();
  this->handle_ = mutex;
  k_mutex_init(mutex);
}
Mutex::~Mutex() { delete static_cast<k_mutex *>(this->handle_); }
void Mutex::lock() { k_mutex_lock(static_cast<k_mutex *>(this->handle_), K_FOREVER); }
bool Mutex::try_lock() { return k_mutex_lock(static_cast<k_mutex *>(this->handle_), K_NO_WAIT) == 0; }
void Mutex::unlock() { k_mutex_unlock(static_cast<k_mutex *>(this->handle_)); }

IRAM_ATTR InterruptLock::InterruptLock() { state_ = irq_lock(); }
IRAM_ATTR InterruptLock::~InterruptLock() { irq_unlock(state_); }

// Zephyr LwIPLock is defined inline as a no-op in helpers.h

bool random_bytes(uint8_t *data, size_t len) {
  sys_rand_get(data, len);
  return true;
}

#ifdef USE_NRF52
void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  mac[0] = ((NRF_FICR->DEVICEADDR[1] & 0xFFFF) >> 8) | 0xC0;
  mac[1] = NRF_FICR->DEVICEADDR[1] & 0xFFFF;
  mac[2] = NRF_FICR->DEVICEADDR[0] >> 24;
  mac[3] = NRF_FICR->DEVICEADDR[0] >> 16;
  mac[4] = NRF_FICR->DEVICEADDR[0] >> 8;
  mac[5] = NRF_FICR->DEVICEADDR[0];
}
#endif
}  // namespace esphome

void setup();
void loop();

int main() {
  setup();
  while (true) {
    loop();
  }
  return 0;
}

#endif
