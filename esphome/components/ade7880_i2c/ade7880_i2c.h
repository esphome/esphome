#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ade7880_base/ade7880_base.h"

namespace esphome {
namespace ade7880_i2c {

class ADE7880I2C : public ade7880_base::ADE7880, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  uint8_t read_u8_register16(uint16_t a_register) override;
  int16_t read_s16_register16(uint16_t a_register) override;
  uint16_t read_u16_register16(uint16_t a_register) override;
  int32_t read_s32_register16(uint16_t a_register) override;
  uint32_t read_u32_register16(uint16_t a_register) override;

  void write_u8_register16(uint16_t a_register, uint8_t value) override;
  void write_s16_register16(uint16_t a_register, int16_t value) override;
  void write_u16_register16(uint16_t a_register, uint16_t value) override;
  void write_s32_register16(uint16_t a_register, int32_t value) override;
  void write_u32_register16(uint16_t a_register, uint32_t value) override;
};

}  // namespace ade7880_i2c
}  // namespace esphome
