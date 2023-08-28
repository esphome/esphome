#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace max7321 {

/// Range for MAX7321 pins
enum MAX7321GPIORange : uint8_t {
  MAX7321_MIN = 0,
  MAX7321_MAX = 7,
};

class MAX7321 : public Component, public i2c::I2CDevice {
 public:
  MAX7321() = default;

  void setup() override;

  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void dump_config() override;

 protected:
  bool write_(uint8_t value);
};

class MAX7321GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(MAX7321 *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

 protected:
  MAX7321 *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace max7321
}  // namespace esphome
