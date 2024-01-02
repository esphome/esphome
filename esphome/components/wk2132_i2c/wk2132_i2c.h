/// @file wk2132_i2c.h
/// @author DrCoolZic
/// @brief  wk2132 classes declaration

#pragma once
#include <bitset>
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/wk2132/wk2132.h"

namespace esphome {
namespace wk2132_i2c {

class WK2132ComponentI2C;

class WK2132RegisterI2C : public wk2132::WK2132Register {
 public:
  uint8_t read_reg() const override;
  void write_reg(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(const uint8_t *data, size_t length) override;

 protected:
  friend WK2132ComponentI2C;
  WK2132RegisterI2C(wk2132::WK2132Component *const comp, uint8_t reg, uint8_t channel)
      : WK2132Register(comp, reg, channel) {}
};

////////////////////////////////////////////////////////////////////////////////////
// class WK2132ComponentI2C
////////////////////////////////////////////////////////////////////////////////////
class WK2132ComponentI2C : public wk2132::WK2132Component, public i2c::I2CDevice {
 public:
  wk2132::WK2132Register &global_reg(uint8_t reg) override {
    reg_i2c_.register_ = reg;
    return reg_i2c_;
  }

  wk2132::WK2132Register &channel_reg(uint8_t reg, uint8_t channel) override {
    reg_i2c_.register_ = reg;
    reg_i2c_.channel_ = channel;
    return reg_i2c_;
  }

  wk2132::WK2132Register &fifo_reg(uint8_t channel) override {
    reg_i2c_.channel_ = channel;
    return reg_i2c_;
  }

  //
  // override Component methods
  //
  void setup() override;
  void dump_config() override;

 protected:
  friend WK2132RegisterI2C;
  uint8_t base_address_;                   ///< base address of I2C device
  WK2132RegisterI2C reg_i2c_{this, 0, 0};  ///< store the current register
};

}  // namespace wk2132_i2c
}  // namespace esphome
