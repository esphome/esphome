/// @file wk2132_spi.h
/// @author DrCoolZic
/// @brief  wk_base classes declaration

#pragma once
#include <bitset>
#include "esphome/components/spi/spi.h"
#include "esphome/components/wk_base/wk_base.h"

namespace esphome {
namespace wk2132_spi {

class WK2132ComponentSPI;

class WK2132RegisterSPI : public wk_base::WKBaseRegister {
 public:
  //
  // implements WKBaseRegister virtual methods
  //
  uint8_t read_reg() const override;
  void write_reg(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(uint8_t *data, size_t length) override;

 protected:
  friend WK2132ComponentSPI;
  /// @brief ctor
  /// @param comp component we belongs to
  /// @param reg proxied register
  /// @param channel associated channel
  WK2132RegisterSPI(wk_base::WKBaseComponent *comp, uint8_t reg, uint8_t channel)
      : WKBaseRegister(comp, reg, channel) {}
};

////////////////////////////////////////////////////////////////////////////////////
// class WK2132ComponentSPI
////////////////////////////////////////////////////////////////////////////////////

/// @brief WKBaseComponent using SPI bus
class WK2132ComponentSPI : public wk_base::WKBaseComponent,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  wk_base::WKBaseRegister &reg(uint8_t reg, uint8_t channel) override {
    reg_spi_.register_ = reg;
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
