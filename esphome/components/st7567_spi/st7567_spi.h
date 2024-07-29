#pragma once

#include "esphome/core/component.h"
#include "esphome/components/st7567_base/st7567_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace st7567_spi {

class SPIST7567 : public st7567_base::ST7567,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }

  void setup() override;

  void dump_config() override;

 protected:
  void command(uint8_t value) override;

  void write_display_data() override;

  GPIOPin *dc_pin_;
};

}  // namespace st7567_spi
}  // namespace esphome
