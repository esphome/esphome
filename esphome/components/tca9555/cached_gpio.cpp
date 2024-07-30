#include "cached_gpio.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tca9555 {

void CachedGpio::setup() { this->cacheInvalidated = false; }

void CachedGpio::loop() { this->cacheInvalidated = false; }

bool CachedGpio::digital_read(uint8_t pin) {
  if (!this->cacheInvalidated) {
    this->cacheInvalidated = true;
    read_gpio_hw();
  }

  return read_gpio_from_cache(pin);
}

void CachedGpio::digital_write(uint8_t pin, bool value) {
  write_gpio_from_cache(pin, value);

  if (!this->cacheInvalidated) {
    this->cacheInvalidated = true;
    write_gpio_hw();
  }
}

}  // namespace tca9555
}  // namespace esphome
