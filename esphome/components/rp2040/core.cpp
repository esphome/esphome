#ifdef USE_RP2040

#include "core.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {

void IRAM_ATTR HOT yield() { ::yield(); }
uint32_t IRAM_ATTR HOT millis() { return ::millis(); }
void IRAM_ATTR HOT delay(uint32_t ms) { ::delay(ms); }
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  // TODO: implement
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}
void arch_init() {}
void IRAM_ATTR HOT arch_feed_wdt() {
  // TODO: implement
}

uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() {
  // TODO: implement
}
uint32_t arch_get_cpu_freq_hz() { return 133 * 1000 * 1000; }

}  // namespace esphome

#endif  // USE_RP2040
