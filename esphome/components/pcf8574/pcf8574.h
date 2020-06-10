#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pcf8574 {

/// Modes for PCF8574 pins
enum PCF8574GPIOMode : uint8_t {
  PCF8574_INPUT = INPUT,
  PCF8574_OUTPUT = OUTPUT,
};

class PCF8574Component : public Component, public i2c::I2CDevice {
 public:
  PCF8574Component() = default;

  void set_pcf8575(bool pcf8575) { pcf8575_ = pcf8575; }

  /// Check i2c availability and setup masks
  void setup() override;
  /// Helper function to read the value of a pin.
  bool digital_read(uint8_t pin);
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, uint8_t mode);

  float get_setup_priority() const override;

  void dump_config() override;

 protected:
  bool read_gpio_();

  bool write_gpio_();

  /// Mask for the pin mode - 1 means output, 0 means input
  uint16_t mode_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state read in read_gpio_ - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};
  bool pcf8575_;  ///< TRUE->16-channel PCF8575, FALSE->8-channel PCF8574
};

/// Helper class to expose a PCF8574 pin as an internal input GPIO pin.
class PCF8574GPIOPin : public GPIOPin {
 public:
  PCF8574GPIOPin(PCF8574Component *parent, uint8_t pin, uint8_t mode, bool inverted = false);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  PCF8574Component *parent_;
};

}  // namespace pcf8574
}  // namespace esphome
