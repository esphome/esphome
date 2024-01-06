/// @file wk2168_i2c.h
/// @author DrCoolZic
/// @brief  wk2168 classes declaration

#pragma once
#include <bitset>
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/wk2168/wk2168.h"

namespace esphome {
namespace wk2168_i2c {

class WK2168ComponentI2C;

////////////////////////////////////////////////////////////////////////////////////
// class WK2168ComponentI2C
////////////////////////////////////////////////////////////////////////////////////
class WK2168RegI2C : public wk2168::WK2168Reg {
 public:
  uint8_t read_reg() const override;
  void write_reg(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(uint8_t *data, size_t length) override;

 protected:
  friend WK2168ComponentI2C;
  WK2168RegI2C(wk2168::WK2168Component *const comp, uint8_t reg, uint8_t channel) : WK2168Reg(comp, reg, channel) {}
};

////////////////////////////////////////////////////////////////////////////////////
// class WK2168ComponentI2C
////////////////////////////////////////////////////////////////////////////////////
class WK2168ComponentI2C : public wk2168::WK2168Component, public i2c::I2CDevice {
 public:
  wk2168::WK2168Reg &reg(uint8_t reg, uint8_t channel) override {
    reg_i2c_.register_ = reg & 0x0F;
    reg_i2c_.channel_ = channel & 0x01;
    return reg_i2c_;
  }

  //
  // override Component methods
  //
  void setup() override;
  void dump_config() override;

 protected:
  friend WK2168RegI2C;
  uint8_t base_address_;              ///< base address of I2C device
  WK2168RegI2C reg_i2c_{this, 0, 0};  ///< store the current register
};

}  // namespace wk2168_i2c
}  // namespace esphome
