#pragma once

#include "esphome/core/component.h"
#include "esphome/components/hp303b/hp303b.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace hp303b_i2c {
static const char *TAG = "hp303b_i2c";
class HP303BComponentI2C : public PollingComponent, public i2c::I2CDevice {
  void setup() override;

  void dump_config() override;
  int16_t set_interrupt_polarity(uint8_t polarity) override;
  int16_t read_byte(uint8_t reg_address) override;
  int16_t read_block(uint8_t reg_address, uint8_t length, uint8_t *buffer) override;
  int16_t write_byte(uint8_t reg_address, uint8_t data, uint8_t check) override;
  int16_t set_interrupt_polarity(uint8_t polarity) override;
}
}  // namespace hp303b_i2c
}  // namespace esphome