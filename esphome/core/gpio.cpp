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

namespace internal {
void pulse_reset_ms(GPIOPin *pin, uint32_t high_before_ms, uint32_t low_ms, uint32_t high_after_ms) {
  if (pin == nullptr)
    return;
  pin->setup();
  const uint32_t waits[] = {high_before_ms, low_ms, high_after_ms};
  // Level for each step is high, low, high; a zero high_before_ms skips the first step
  for (size_t i = high_before_ms == 0; i < 3; i++) {
    pin->digital_write(i != 1);
    delay(waits[i]);
  }
}
}  // namespace internal

}  // namespace esphome
