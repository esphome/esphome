/// @file weikai_i2c.h
/// @author DrCoolZic
/// @brief  WeiKai component family - classes declaration
/// @date Last Modified: 2024/03/01 13:31:57
/// @details The classes declared in this file can be used by the Weikai family
///          wk2132_i2c, wk2168_i2c, wk2204_i2c, wk2212_i2c

#pragma once
#include <bitset>
#include <memory>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/weikai/weikai.h"

namespace esphome {
namespace weikai_i2c {

class WeikaiComponentI2C;

// using namespace weikai;
////////////////////////////////////////////////////////////////////////////////////
/// @brief WeikaiRegisterI2C objects acts as proxies to access remote register through an I2C Bus
////////////////////////////////////////////////////////////////////////////////////
class WeikaiRegisterI2C : public weikai::WeikaiRegister {
 public:
  uint8_t read_reg() const override;
  void write_reg(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(uint8_t *data, size_t length) override;

 protected:
  friend WeikaiComponentI2C;
  WeikaiRegisterI2C(weikai::WeikaiComponent *const comp, uint8_t reg, uint8_t channel)
      : weikai::WeikaiRegister(comp, reg, channel) {}
};

////////////////////////////////////////////////////////////////////////////////////
/// @brief The WeikaiComponentI2C class stores the information to the WeiKai component
/// connected through an I2C bus.
////////////////////////////////////////////////////////////////////////////////////
class WeikaiComponentI2C : public weikai::WeikaiComponent, public i2c::I2CDevice {
 public:
  weikai::WeikaiRegister &reg(uint8_t reg, uint8_t channel) override {
    reg_i2c_.register_ = reg;
    reg_i2c_.channel_ = channel;
    return reg_i2c_;
  }

  //
  // override Component methods
  //
  void setup() override;
  void dump_config() override;

  uint8_t base_address_;                   ///< base address of I2C device
  WeikaiRegisterI2C reg_i2c_{this, 0, 0};  ///< init to this component
};

}  // namespace weikai_i2c
}  // namespace esphome
