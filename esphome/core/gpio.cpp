#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

namespace esphome {

#ifdef USE_ESP8266
void log_pin(const char *tag, const __FlashStringHelper *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  static constexpr size_t LOG_PIN_PREFIX_MAX_LEN = 32;
  char prefix_buf[LOG_PIN_PREFIX_MAX_LEN];
  strncpy_P(prefix_buf, reinterpret_cast<const char *>(prefix), sizeof(prefix_buf) - 1);
  prefix_buf[sizeof(prefix_buf) - 1] = '\0';
  log_pin_with_prefix(tag, prefix_buf, pin);
}
#else
void log_pin(const char *tag, const char *prefix, GPIOPin *pin) {
  if (pin == nullptr)
    return;
  log_pin_with_prefix(tag, prefix, pin);
}
#endif

}  // namespace esphome
