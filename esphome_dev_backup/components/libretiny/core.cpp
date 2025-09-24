#ifdef USE_LIBRETINY

#include "core.h"
#include "esphome/core/defines.h"
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
void IRAM_ATTR HOT arch_feed_wdt() { lt_wdt_feed(); }
uint32_t arch_get_cpu_cycle_count() { return lt_cpu_get_cycle_count(); }
uint32_t arch_get_cpu_freq_hz() { return lt_cpu_get_freq(); }
uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }

}  // namespace esphome

#endif  // USE_LIBRETINY
