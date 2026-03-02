#ifdef USE_LIBRETINY

#include "core.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/time_64.h"
#include "esphome/core/helpers.h"
#include "preferences.h"

void setup();
void loop();

namespace esphome {

void HOT yield() { ::yield(); }
uint32_t IRAM_ATTR HOT millis() { return ::millis(); }
uint64_t millis_64() { return Millis64Impl::compute(::millis()); }
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void HOT delay(uint32_t ms) { ::delay(ms); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }

void arch_init() {
  libretiny::setup_preferences();
  lt_wdt_enable(10000L);
#if LT_GPIO_RECOVER
  lt_gpio_recover();
#endif
}

void arch_restart() {
  lt_reboot();
  while (1) {
  }
}
void HOT arch_feed_wdt() { lt_wdt_feed(); }
uint32_t arch_get_cpu_cycle_count() { return lt_cpu_get_cycle_count(); }
uint32_t arch_get_cpu_freq_hz() { return lt_cpu_get_freq(); }
uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
uint16_t progmem_read_uint16(const uint16_t *addr) { return *addr; }

}  // namespace esphome

#endif  // USE_LIBRETINY
