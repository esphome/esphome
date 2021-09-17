#ifdef USE_ESP8266

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"
#include <Arduino.h>
#include <Esp.h>

namespace esphome {

void IRAM_ATTR HOT yield() { ::yield(); }
uint32_t IRAM_ATTR HOT millis() { return ::millis(); }
void IRAM_ATTR HOT delay(uint32_t ms) { ::delay(ms); }
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }
void arch_restart() {
  ESP.restart();  // NOLINT(readability-static-accessed-through-instance)
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}
void IRAM_ATTR HOT arch_feed_wdt() {
  ESP.wdtFeed();  // NOLINT(readability-static-accessed-through-instance)
}

uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
uint32_t arch_get_cpu_cycle_count() {
  return ESP.getCycleCount();  // NOLINT(readability-static-accessed-through-instance)
}
uint32_t arch_get_cpu_freq_hz() { return F_CPU; }

}  // namespace esphome

#endif  // USE_ESP8266
