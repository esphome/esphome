#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pca6416a {

class PCA6416AComponent : public Component, public i2c::I2CDevice {
 public:
  PCA6416AComponent() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  /// Helper function to read the value of a pin.
  bool digital_read(uint8_t pin);
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;

  void dump_config() override;

 protected:
  bool read_register_(uint8_t reg, uint8_t *value);
  bool write_register_(uint8_t reg, uint8_t value);
  void update_register_(uint8_t pin, bool pin_value, uint8_t reg_addr);

  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint8_t output_0_{0x00};
  uint8_t output_1_{0x00};
  /// Storage for last I2C error seen
  esphome::i2c::ErrorCode last_error_;
  /// Only the PCAL6416A has pull-up resistors
  bool has_pullup_{false};
};

/// Helper class to expose a PCA6416A pin as an internal input GPIO pin.
class PCA6416AGPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(PCA6416AComponent *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

 protected:
  PCA6416AComponent *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace pca6416a
}  // namespace esphome
