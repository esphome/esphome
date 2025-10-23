#include "gpio_expander_test_component_uint16.h"
#include "esphome/core/log.h"

namespace esphome::gpio_expander_test_component_uint16 {

static const char *const TAG = "gpio_expander_test_uint16";

void GPIOExpanderTestUint16Component::setup() {
  ESP_LOGD(TAG, "Testing uint16_t bank (single 16-pin bank)");

  // Test reading all 16 pins - first should trigger hw read, rest use cache
  for (uint8_t pin = 0; pin < 16; pin++) {
    this->digital_read(pin);
  }

  // Reset cache and test specific reads
  ESP_LOGD(TAG, "Resetting cache for uint16_t test");
  this->reset_pin_cache_();

  // First read triggers hw for entire bank
  this->digital_read(5);
  // These should all use cache since they're in the same bank
  this->digital_read(10);
  this->digital_read(15);
  this->digital_read(0);

  ESP_LOGD(TAG, "DONE_UINT16");
}

bool GPIOExpanderTestUint16Component::digital_read_hw(uint8_t pin) {
  ESP_LOGD(TAG, "uint16_digital_read_hw pin=%d", pin);
  // In a real component, this would read from I2C/SPI into internal state
  // For testing, we just return true to indicate successful read
  return true;  // Return true to indicate successful read
}

bool GPIOExpanderTestUint16Component::digital_read_cache(uint8_t pin) {
  ESP_LOGD(TAG, "uint16_digital_read_cache pin=%d", pin);
  // Return the actual pin state from our test pattern
  return (this->test_state_ >> pin) & 1;
}

}  // namespace esphome::gpio_expander_test_component_uint16
