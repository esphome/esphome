#include "cached_gpio.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tca9555 {

CachedGpio::CachedGpio(CachedGpioClient *client) {
  this->client_ = client;
  this->read_cache_invalidated_ = false;
  this->write_cache_invalidated_ = false;
};

void CachedGpio::loop() {
  this->read_cache_invalidated_ = false;
  this->write_cache_invalidated_ = false;
}

bool CachedGpio::digital_read(uint8_t pin) {
  if (!this->read_cache_invalidated_) {
    this->read_cache_invalidated_ = true;
    this->client_->read_gpio_hw();
  }

  return this->client_->read_gpio_from_cache(pin);
}

void CachedGpio::digital_write(uint8_t pin, bool value) {
  this->client_->write_gpio_from_cache(pin, value);

  if (!this->write_cache_invalidated_) {
    this->write_cache_invalidated_ = true;
    this->client_->write_gpio_hw();
  }
}

}  // namespace tca9555
}  // namespace esphome
