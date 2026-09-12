#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sh1122_base/sh1122_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace sh1122_spi {

class SPISH1122 : public sh1122_base::SH1122,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }

  void setup() override;

  void dump_config() override;

 protected:
  void command(uint8_t value) override;
  void command2(uint8_t value, uint8_t data) override;
  void data(uint8_t value) override;

  void write_display_data() override;

  GPIOPin *dc_pin_;
};

}  // namespace sh1122_spi
}  // namespace esphome
