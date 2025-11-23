#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/core/component.h"

namespace esphome::gpio_expander_test_component_uint16 {

// Test component using uint16_t bank type (single 16-pin bank)
class GPIOExpanderTestUint16Component : public Component,
                                        public esphome::gpio_expander::CachedGpioExpander<uint16_t, 16> {
 public:
  void setup() override;

 protected:
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override{};

 private:
  uint16_t test_state_{0xAAAA};  // Test pattern: alternating bits
};

}  // namespace esphome::gpio_expander_test_component_uint16
