#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::pcf8574 {

// PCF8574(8 pins)/PCF8575(16 pins) always read/write all pins in a single I2C transaction
// so we use uint16_t as bank type to ensure all pins are in one bank and cached together
class PCF8574Component final : public gpio_expander::CachedGpioExpanderComponent<uint16_t, 16>, public i2c::I2CDevice {
 public:
  PCF8574Component() = default;

  void set_pcf8575(bool pcf8575) { pcf8575_ = pcf8575; }
  void set_reset(bool reset) { this->reset_ = reset; }
  // Called by pin setters before any component's setup or GPIO writes.
  void register_input_pin(uint8_t pin) { this->configured_input_pins_ |= 1 << pin; }

  /// Check i2c availability and setup masks
  void setup() override;
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  void dump_config() override;

 protected:
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  bool read_gpio_();
  bool write_gpio_();

  /// Mask for the pin mode - 1 means output, 0 means input
  uint16_t mode_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state read in read_gpio_ - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};
  // Pins configured as inputs before setup; 1 means input. With reset disabled, all other pins
  // (including undeclared pins) are treated as outputs seeded from their observed levels.
  uint16_t configured_input_pins_{0x00};
  // Keep in sync with DEFAULT_RESET in __init__.py; codegen omits the setter for the default.
  bool reset_{true};
  bool pcf8575_;  ///< TRUE->16-channel PCF8575, FALSE->8-channel PCF8574
};

using PCF8574GPIOPin = gpio_expander::ExpanderGPIOPin<PCF8574Component>;

}  // namespace esphome::pcf8574

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::pcf8574::PCF8574Component);
