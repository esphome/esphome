#include "gpio_expander_test_component.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::gpio_expander_test_component {

static const char *const TAG = "gpio_expander_test";

void GPIOExpanderTestComponent::setup() {
  for (uint8_t pin = 0; pin < 32; pin++) {
    this->digital_read(pin);
  }

  this->digital_read(3);
  this->digital_read(3);
  this->digital_read(4);
  this->digital_read(3);
  this->digital_read(10);
  this->reset_pin_cache_();  // Reset cache to ensure next read is from hardware
  this->digital_read(15);
  this->digital_read(14);
  this->digital_read(14);

  ESP_LOGD(TAG, "DONE");
}

bool GPIOExpanderTestComponent::digital_read_hw(uint8_t pin) {
  ESP_LOGD(TAG, "digital_read_hw pin=%d", pin);
  return true;
}

bool GPIOExpanderTestComponent::digital_read_cache(uint8_t pin) {
  ESP_LOGD(TAG, "digital_read_cache pin=%d", pin);
  return true;
}

}  // namespace esphome::gpio_expander_test_component
