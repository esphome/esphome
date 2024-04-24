#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace xl9535 {

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

class XL9535Component : public Component, public i2c::I2CDevice {
 public:
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, gpio::Flags mode);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
};

class XL9535GPIOPin : public GPIOPin {
 public:
  void set_parent(XL9535Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  std::string dump_summary() const override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  XL9535Component *parent_;

  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace xl9535
}  // namespace esphome
