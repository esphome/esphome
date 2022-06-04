#ifdef USE_LIBRETUYA

#include "core.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"

void setup();
void loop();

namespace esphome {

void IRAM_ATTR HOT yield() { ::yield(); }
uint32_t IRAM_ATTR HOT millis() { return ::millis(); }
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void IRAM_ATTR HOT delay(uint32_t ms) { ::delay(ms); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }

void arch_restart() {
  //
  LT.restart();
}

void arch_init() {}

void IRAM_ATTR HOT arch_feed_wdt() {
  // TODO reset watchdog
}

uint8_t progmem_read_byte(const uint8_t *addr) {
  //
  return *addr;
}

uint32_t arch_get_cpu_cycle_count() {
  //
  return LT.getCycleCount();
}
uint32_t arch_get_cpu_freq_hz() {
  //
  return LT.getCpuFreq();
}

#ifdef USE_ARDUINO
extern "C" void init() {
  //
  libretuya::setup_preferences();
}
#endif  // USE_ARDUINO

}  // namespace esphome

#endif  // USE_LIBRETUYA
