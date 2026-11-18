#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::xl9535 {

enum {
  XL9535_INPUT_PORT_0_REGISTER = 0x00,
  XL9535_INPUT_PORT_1_REGISTER = 0x01,
  XL9535_OUTPUT_PORT_0_REGISTER = 0x02,
  XL9535_OUTPUT_PORT_1_REGISTER = 0x03,
  XL9535_INVERSION_PORT_0_REGISTER = 0x04,
  XL9535_INVERSION_PORT_1_REGISTER = 0x05,
  XL9535_CONFIG_PORT_0_REGISTER = 0x06,
  XL9535_CONFIG_PORT_1_REGISTER = 0x07,
};

class XL9535Component final : public Component, public i2c::I2CDevice {
 public:
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, gpio::Flags mode);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
};

using XL9535GPIOPin = gpio_expander::ExpanderGPIOPin<XL9535Component>;

}  // namespace esphome::xl9535

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::xl9535::XL9535Component);
