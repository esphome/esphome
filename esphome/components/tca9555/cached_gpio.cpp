#include "cached_gpio.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tca9555 {

CachedGpio::CachedGpio() {
  readCacheInvalidated = false;
  writeCacheInvalidated = false;
};

void CachedGpio::loop() {
  this->readCacheInvalidated = false;
  this->writeCacheInvalidated = false;
}

bool CachedGpio::digital_read(uint8_t pin) {
  if (!this->readCacheInvalidated) {
    this->readCacheInvalidated = true;
    this->read_gpio_hw();
  }

  return this->read_gpio_from_cache(pin);
}

void CachedGpio::digital_write(uint8_t pin, bool value) {
  this->write_gpio_from_cache(pin, value);

  if (!this->writeCacheInvalidated) {
    this->writeCacheInvalidated = true;
    this->write_gpio_hw();
  }
}

}  // namespace tca9555
}  // namespace esphome
