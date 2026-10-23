#pragma once

#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ch423 {

class CH423Component final : public Component, public i2c::I2CDevice {
 public:
  CH423Component() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  /// Poll for input changes periodically
  void loop() override;
  /// Helper function to read the value of a pin.
  bool digital_read(uint8_t pin);
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  bool write_reg_(uint8_t reg, uint8_t value);
  uint8_t read_reg_(uint8_t reg);
  bool read_inputs_();
  bool write_outputs_();

  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint32_t output_bits_{0x00};
  /// Flags to check if read previously during this loop
  uint8_t pin_read_flags_{0x00};
  /// Copy of last read values
  uint8_t input_bits_{0x00};
  /// System parameters
  uint8_t sys_params_{0x00};
};

/// Helper class to expose a CH423 pin as a GPIO pin.
using CH423GPIOPin = gpio_expander::ExpanderGPIOPin<CH423Component>;

}  // namespace esphome::ch423

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::ch423::CH423Component);
