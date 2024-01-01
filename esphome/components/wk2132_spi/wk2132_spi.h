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
  //
  // implements WK2132Register virtual methods
  //
  uint8_t get() const override;
  void set(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(const uint8_t *data, size_t length) override;

 protected:
  friend WK2132ComponentSPI;
  /// @brief ctor
  /// @param comp component we belongs to
  /// @param reg proxied register
  /// @param channel associated channel
  WK2132RegisterSPI(wk2132::WK2132Component *comp, uint8_t reg, uint8_t channel) : WK2132Register(comp, reg, channel) {}
};

////////////////////////////////////////////////////////////////////////////////////
// class WK2132ComponentSPI
////////////////////////////////////////////////////////////////////////////////////

/// @brief WK2132Component using SPI bus
class WK2132ComponentSPI : public wk2132::WK2132Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  wk2132::WK2132Register &global_reg(uint8_t reg) override {
    reg_spi_.register_ = reg;
    return reg_spi_;
  }

  wk2132::WK2132Register &channel_reg(uint8_t reg, uint8_t channel) override {
    reg_spi_.register_ = reg;
    reg_spi_.channel_ = channel;
    return reg_spi_;
  }

  wk2132::WK2132Register &fifo_reg(uint8_t channel) override {
    // reg_spi_.register_ = 0;
    reg_spi_.channel_ = channel;
    return reg_spi_;
  }

  //
  // override Component methods
  //
  void setup() override;
  void dump_config() override;

 protected:
  friend WK2132RegisterSPI;
  WK2132RegisterSPI reg_spi_{this, 0, 0};  ///< store the current register
};

}  // namespace wk2132_spi
}  // namespace esphome
