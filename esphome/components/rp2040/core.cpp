#ifdef USE_RP2040

#include "core.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include "hardware/timer.h"
#include "hardware/watchdog.h"

namespace esphome {

void HOT yield() { ::yield(); }
uint64_t millis_64() { return time_us_64() / 1000ULL; }
uint32_t HOT millis() { return static_cast<uint32_t>(millis_64()); }
void HOT delay(uint32_t ms) { ::delay(ms); }
uint32_t HOT micros() { return ::micros(); }
void HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  watchdog_reboot(0, 0, 10);
  while (1) {
    continue;
  }
}

void arch_init() {
#if USE_RP2040_WATCHDOG_TIMEOUT > 0
  watchdog_enable(USE_RP2040_WATCHDOG_TIMEOUT, false);
#endif
}

void HOT arch_feed_wdt() { watchdog_update(); }

uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
uint16_t progmem_read_uint16(const uint16_t *addr) { return *addr; }
uint32_t HOT arch_get_cpu_cycle_count() { return ulMainGetRunTimeCounterValue(); }
uint32_t arch_get_cpu_freq_hz() { return RP2040::f_cpu(); }

}  // namespace esphome

#endif  // USE_RP2040
