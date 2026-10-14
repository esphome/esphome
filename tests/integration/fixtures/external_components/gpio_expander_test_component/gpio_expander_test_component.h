#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/core/component.h"

namespace esphome::gpio_expander_test_component {

class GPIOExpanderTestComponent : public Component, public esphome::gpio_expander::CachedGpioExpander<uint8_t, 32> {
 public:
  void setup() override;

 protected:
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override{};
};

}  // namespace esphome::gpio_expander_test_component
