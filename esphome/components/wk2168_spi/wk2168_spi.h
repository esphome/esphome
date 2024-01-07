/// @file wk2168_spi.h
/// @author DrCoolZic
/// @brief  wk2168 classes declaration

#pragma once
#include <bitset>
#include "esphome/components/spi/spi.h"
#include "esphome/components/wk2168/wk2168.h"

namespace esphome {
namespace wk2168_spi {

////////////////////////////////////////////////////////////////////////////////////
// class WK2168ComponentSPI
////////////////////////////////////////////////////////////////////////////////////
class WK2168RegSPI : public wk_base::WKBaseRegister {
 public:
  uint8_t read_reg() const override;
  void write_reg(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(uint8_t *data, size_t length) override;

 protected:
  friend WK2168ComponentSPI;
  WK2168RegSPI(wk2168::WK2168Component *const comp, uint8_t reg, uint8_t channel)
      : WKBaseRegister(comp, reg, channel) {}
};

////////////////////////////////////////////////////////////////////////////////////
// class WK2168ComponentSPI
////////////////////////////////////////////////////////////////////////////////////

/// @brief WK2168Component using SPI bus
class WK2168ComponentSPI : public wk2168::WK2168Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  wk_base::WKBaseRegister &reg(uint8_t reg, uint8_t channel) override {
    reg_spi_.register_ = reg & 0x0F;
    reg_spi_.channel_ = channel & 0x01;
    return reg_spi_;
  }

  void setup() override;
  void dump_config() override;

 protected:
  friend class WK2168RegSPI;
  WK2168RegSPI reg_spi_{this, 0, 0};  ///< store the current register
};

}  // namespace wk2168_spi
}  // namespace esphome
