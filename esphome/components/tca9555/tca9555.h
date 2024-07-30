#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "cached_gpio.h"

namespace esphome {
namespace tca9555 {

class TCA9555Component : virtual public Component, public i2c::I2CDevice, public CachedGpio {
 public:
  TCA9555Component() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;

  void dump_config() override;

 protected:
  /// Mask for the pin mode - 1 means output, 0 means input
  uint16_t mode_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state read in read_gpio_hw - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};

  bool read_gpio_from_cache(uint8_t pin);
  bool write_gpio_from_cache(uint8_t pin, bool value);
  bool read_gpio_hw();
  bool write_gpio_hw();

 private:
  bool read_gpio_current_config();
};

/// Helper class to expose a TCA9555 pin as an internal input GPIO pin.
class TCA9555GPIOPin : public GPIOPin, public Parented<TCA9555Component> {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

 protected:
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace tca9555
}  // namespace esphome
