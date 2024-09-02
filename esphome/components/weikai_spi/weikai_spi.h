/// @file weikai.h
/// @author DrCoolZic
/// @brief  WeiKai component family - classes declaration
/// @date Last Modified: 2024/02/29 17:20:32
/// @details The classes declared in this file can be used by the Weikai family
///          wk2124_spi, wk2132_spi, wk2168_spi, wk2204_spi, wk2212_spi,

#pragma once
#include <bitset>
#include <memory>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/weikai/weikai.h"

namespace esphome {
namespace weikai_spi {
////////////////////////////////////////////////////////////////////////////////////
/// @brief WeikaiRegisterSPI objects acts as proxies to access remote register through an SPI Bus
////////////////////////////////////////////////////////////////////////////////////
class WeikaiRegisterSPI : public weikai::WeikaiRegister {
 public:
  WeikaiRegisterSPI(weikai::WeikaiComponent *const comp, uint8_t reg, uint8_t channel)
      : weikai::WeikaiRegister(comp, reg, channel) {}

  uint8_t read_reg() const override;
  void write_reg(uint8_t value) override;
  void read_fifo(uint8_t *data, size_t length) const override;
  void write_fifo(uint8_t *data, size_t length) override;
};

////////////////////////////////////////////////////////////////////////////////////
/// @brief The WeikaiComponentSPI class stores the information to the WeiKai component
/// connected through an SPI bus.
////////////////////////////////////////////////////////////////////////////////////
class WeikaiComponentSPI : public weikai::WeikaiComponent,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  weikai::WeikaiRegister &reg(uint8_t reg, uint8_t channel) override {
    reg_spi_.register_ = reg;
    reg_spi_.channel_ = channel;
    return reg_spi_;
  }

  void setup() override;
  void dump_config() override;

 protected:
  WeikaiRegisterSPI reg_spi_{this, 0, 0};  ///< init to this component
};

}  // namespace weikai_spi
}  // namespace esphome
