#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace aw9523 {

static const uint8_t AW9523_REG_CHIPID = 0x10;
static const uint8_t AW9523_REG_SOFTRESET = 0x7F;
static const uint8_t AW9523_REG_INPUT0 = 0x00;
static const uint8_t AW9523_REG_INPUT1 = 0x01;
static const uint8_t AW9523_REG_OUTPUT0 = 0x02;
static const uint8_t AW9523_REG_OUTPUT1 = 0x03;
static const uint8_t AW9523_REG_CONFIG0 = 0x04;
static const uint8_t AW9523_REG_CONFIG1 = 0x05;
static const uint8_t AW9523_REG_INTENABLE0 = 0x06;
static const uint8_t AW9523_REG_INTENABLE1 = 0x07;
static const uint8_t AW9523_REG_GCR = 0x11;
static const uint8_t AW9523_REG_LEDMODE0 = 0x12;
static const uint8_t AW9523_REG_LEDMODE1 = 0x13;

class AW9523Component : public Component, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::IO; }
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_divider(uint8_t divider);
  uint8_t get_divider();
  void set_latch_inputs(bool latch_inputs) { this->latch_inputs_ = latch_inputs; }
  float get_max_current();
  void led_driver(uint8_t pin);
  void set_pin_value(uint8_t pin, uint8_t val);
  void pin_mode(uint8_t pin, gpio::Flags flags);
  void digital_write(uint8_t pin, bool bit_value);
  bool digital_read(uint8_t pin);

 protected:
  uint16_t value_{};
  uint8_t divider_{};
  bool latch_inputs_{};
};

}  // namespace aw9523
}  // namespace esphome
