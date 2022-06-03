#ifdef USE_LIBRETUYA

#include "core.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"

void setup();
void loop();

namespace esphome {

void arch_restart() {
  // TODO restart system
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}

void arch_init() {}

void IRAM_ATTR HOT arch_feed_wdt() {
  // TODO reset watchdog
}

uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }

uint32_t arch_get_cpu_cycle_count() {
  // TODO return CPU cycle count
  return 0;
}
uint32_t arch_get_cpu_freq_hz() {
  // TODO return CPU clock
  return 0;
}

#ifdef USE_ARDUINO
extern "C" void init() {
  //
  libretuya::setup_preferences();
}
#endif  // USE_ARDUINO

}  // namespace esphome

#endif  // USE_LIBRETUYA
