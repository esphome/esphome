#ifdef USE_ESP8266

#include "core.h"
#include "esphome/core/defines.h"
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
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  ESP.restart();  // NOLINT(readability-static-accessed-through-instance)
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}
void arch_init() {}
void IRAM_ATTR HOT arch_feed_wdt() {
  ESP.wdtFeed();  // NOLINT(readability-static-accessed-through-instance)
}

uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() {
  return ESP.getCycleCount();  // NOLINT(readability-static-accessed-through-instance)
}
uint32_t arch_get_cpu_freq_hz() { return F_CPU; }

void force_link_symbols() {
  // Tasmota uses magic bytes in the binary to check if an OTA firmware is compatible
  // with their settings - ESPHome uses a different settings system (that can also survive
  // erases). So set magic bytes indicating all tasmota versions are supported.
  // This only adds 12 bytes of binary size, which is an acceptable price to pay for easier support
  // for Tasmota.
  // https://github.com/arendst/Tasmota/blob/b05301b1497942167a015a6113b7f424e42942cd/tasmota/settings.ino#L346-L380
  // https://github.com/arendst/Tasmota/blob/b05301b1497942167a015a6113b7f424e42942cd/tasmota/i18n.h#L652-L654
  const static uint32_t TASMOTA_MAGIC_BYTES[] PROGMEM = {0x5AA55AA5, 0xFFFFFFFF, 0xA55AA55A};
  // Force link symbol by using a volatile integer (GCC attribute used does not work because of LTO)
  volatile int x = 0;
  x = TASMOTA_MAGIC_BYTES[x];
}

extern "C" void resetPins() {  // NOLINT
  // Added in framework 2.7.0
  // usually this sets up all pins to be in INPUT mode
  // however, not strictly needed as we set up the pins properly
  // ourselves and this causes pins to toggle during reboot.
  force_link_symbols();

#ifdef USE_ESP8266_EARLY_PIN_INIT
  for (int i = 0; i < 16; i++) {
    uint8_t mode = ESPHOME_ESP8266_GPIO_INITIAL_MODE[i];
    uint8_t level = ESPHOME_ESP8266_GPIO_INITIAL_LEVEL[i];
    if (mode != 255)
      pinMode(i, mode);  // NOLINT
    if (level != 255)
      digitalWrite(i, level);  // NOLINT
  }
#endif
}

}  // namespace esphome

#endif  // USE_ESP8266
