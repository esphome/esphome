/// @file wk2132_spi.h
/// @author DrCoolZic
/// @brief  wk2132 classes declaration

#pragma once
#include <bitset>
#include "esphome/components/spi/spi.h"
#include "esphome/components/wk2132/wk2132.h"

namespace esphome {
namespace wk2132_spi {

class WK2132ComponentSPI;

class WK2132RegisterSPI : public wk2132::WK2132Register {
 public:
  uint8_t get() const override;
  void set(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(const uint8_t *data, size_t length) override;

 protected:
  friend WK2132ComponentSPI;
  WK2132RegisterSPI(wk2132::WK2132Component *parent, uint8_t reg, uint8_t channel, uint8_t fifo)
      : WK2132Register(parent, reg, channel, fifo) {}
};

// class WK2132Channel;  // forward declaration

////////////////////////////////////////////////////////////////////////////////////
// class WK2132ComponentSPI
////////////////////////////////////////////////////////////////////////////////////
class WK2132ComponentSPI : public wk2132::WK2132Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  // std::unique_ptr<wk2132::WK2132Register> global_reg_ptr(uint8_t reg) override {
  //   std::unique_ptr<wk2132::WK2132Register> r(new WK2132RegisterSPI{this, reg, 0, 0});
  //   return r;
  // }
  wk2132::WK2132Register &global_reg(uint8_t reg) override {
    registerI2C_.register_ = reg;
    return registerI2C_;
  }

  std::unique_ptr<wk2132::WK2132Register> channel_reg_ptr(uint8_t reg, uint8_t channel) {
    std::unique_ptr<wk2132::WK2132Register> r(new WK2132RegisterSPI{this, reg, channel, 0});
    return r;
  }

  std::unique_ptr<wk2132::WK2132Register> fifo_reg_ptr(uint8_t channel) {
    std::unique_ptr<wk2132::WK2132Register> r(new WK2132RegisterSPI{this, 0, channel, 1});
    return r;
  }

  //
  // override Component methods
  //
  void setup() override;
  void dump_config() override;

 protected:
  friend WK2132RegisterSPI;
  uint8_t base_address_;  ///< base address of I2C device
  WK2132RegisterSPI registerI2C_;
};

}  // namespace wk2132_spi
}  // namespace esphome
