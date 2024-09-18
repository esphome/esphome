#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ch422g {

class CH422GComponent : public Component, public i2c::I2CDevice {
 public:
  CH422GComponent() = default;

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

 protected:
  bool write_reg_(uint8_t reg, uint8_t value);
  uint8_t read_reg_(uint8_t reg);
  bool set_mode_(uint8_t mode);
  bool read_inputs_();
  bool write_outputs_();

  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_bits_{0x00};
  /// Flags to check if read previously during this loop
  uint8_t pin_read_flags_ = {0x00};
  /// Copy of last read values
  uint8_t input_bits_ = {0x00};
  /// Copy of the mode value
  uint8_t mode_value_{};
};

/// Helper class to expose a CH422G pin as a GPIO pin.
class CH422GGPIOPin : public GPIOPin {
 public:
  void setup() override{};
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(CH422GComponent *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags);

 protected:
  CH422GComponent *parent_{};
  uint8_t pin_{};
  bool inverted_{};
  gpio::Flags flags_{};
};

}  // namespace ch422g
}  // namespace esphome
