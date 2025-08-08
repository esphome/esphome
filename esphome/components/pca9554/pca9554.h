#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pca9554 {

class PCA9554Component : public Component, public i2c::I2CDevice {
 public:
  PCA9554Component() = default;

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

  float get_loop_priority() const override;

  void dump_config() override;

  void set_pin_count(size_t pin_count) { this->pin_count_ = pin_count; }

 protected:
  bool read_inputs_();

  bool write_register_(uint8_t reg, uint16_t value);

  /// number of bits the expander has
  size_t pin_count_{8};
  /// width of registers
  size_t reg_width_{1};
  /// Mask for the pin config - 1 means OUTPUT, 0 means INPUT
  uint16_t config_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state of the actual input pin states - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};
  /// Flags to check if read previously during this loop
  uint16_t was_previously_read_ = {0x00};
  /// Storage for last I2C error seen
  esphome::i2c::ErrorCode last_error_;
};

/// Helper class to expose a PCA9554 pin as an internal input GPIO pin.
class PCA9554GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(PCA9554Component *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  PCA9554Component *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace pca9554
}  // namespace esphome
