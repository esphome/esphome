#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {

#ifdef USE_ESP8266
void log_pin(const char *tag, const __FlashStringHelper *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  // %s reads flash on ESP8266, so the prefix needs no RAM copy
  log_pin_with_prefix(tag, reinterpret_cast<const char *>(prefix), pin);
}
#else
void log_pin(const char *tag, const char *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  log_pin_with_prefix(tag, prefix, pin);
}
#endif

void pulse_reset(GPIOPin *pin, uint32_t low_ms, uint32_t settle_ms, uint32_t pre_high_ms) {
  if (pin == nullptr)
    return;
  pin->setup();
  if (pre_high_ms != 0) {
    pin->digital_write(true);
    delay(pre_high_ms);
  }
  pin->digital_write(false);
  delay(low_ms);
  pin->digital_write(true);
  delay(settle_ms);
}

}  // namespace esphome
