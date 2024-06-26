#ifdef USE_RP2040

#include "core.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include "hardware/watchdog.h"

namespace esphome {

void IRAM_ATTR HOT yield() { ::yield(); }
uint32_t IRAM_ATTR HOT millis() { return ::millis(); }
void IRAM_ATTR HOT delay(uint32_t ms) { ::delay(ms); }
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  watchdog_reboot(0, 0, 10);
  while (1) {
    continue;
  }
}
void arch_init() { watchdog_enable(0x7fffff, false); }
void IRAM_ATTR HOT arch_feed_wdt() { watchdog_update(); }

uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() { return ulMainGetRunTimeCounterValue(); }
uint32_t arch_get_cpu_freq_hz() { return RP2040::f_cpu(); }

}  // namespace esphome

#endif  // USE_RP2040
