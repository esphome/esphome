/// @file wk2132_i2c.h
/// @author DrCoolZic
/// @brief  wk2132 classes declaration

#pragma once
#include <bitset>
// #include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/wk2132/wk2132.h"
// #include "esphome/components/uart/uart.h"
// #if defined(USE_ESP32_FRAMEWORK_ARDUINO)
// #include "Wire.h"
// #endif

namespace esphome {
namespace wk2132_i2c {

class WK2132ComponentI2C;

class WK2132RegisterI2C : public wk2132::WK2132Register {
 public:
  uint8_t get() const override;
  void set(uint8_t value) override;
  void get_array(uint8_t *data, size_t length) override;
  void set_array(const uint8_t *data, size_t length) override;

 protected:
  // friend class WK2132Component;
  // friend class WK2132Channel;
  friend WK2132ComponentI2C;

  // using WK2132Register::WK2132Register;
  WK2132RegisterI2C(wk2132::WK2132Component *parent, uint8_t reg, uint8_t channel, uint8_t fifo)
      : WK2132Register(parent, reg, channel, fifo) {}
};

// class WK2132Channel;  // forward declaration

////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
class WK2132ComponentI2C : public wk2132::WK2132Component, public i2c::I2CDevice {
 public:
  std::unique_ptr<wk2132::WK2132Register> global_reg_ptr(uint8_t reg) override {
    std::unique_ptr<wk2132::WK2132Register> r(new WK2132RegisterI2C{this, reg, 0, 0});
    return r;
  }

  std::unique_ptr<wk2132::WK2132Register> channel_reg_ptr(uint8_t reg, uint8_t channel) {
    std::unique_ptr<wk2132::WK2132Register> r(new WK2132RegisterI2C{this, reg, channel, 0});
    return r;
  }

  std::unique_ptr<wk2132::WK2132Register> fifo_reg_ptr(uint8_t channel) {
    std::unique_ptr<wk2132::WK2132Register> r(new WK2132RegisterI2C{this, 0, channel, 1});
    return r;
  }

  void setup() override;
  void dump_config() override;

 protected:
  friend WK2132RegisterI2C;
  uint8_t base_address_;  ///< base address of I2C device
  bool page1_{false};     ///< set to true when in "page1 mode"
};

}  // namespace wk2132_i2c
}  // namespace esphome
