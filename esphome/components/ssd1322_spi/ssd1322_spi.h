#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ssd1322_base/ssd1322_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace ssd1322_spi {

class SPISSD1322 : public ssd1322_base::SSD1322,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                         spi::DATA_RATE_8MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }

  void setup() override;

  void dump_config() override;

 protected:
  void command(uint8_t value) override;
  void data(uint8_t value) override;

  void write_display_data() override;

  GPIOPin *dc_pin_;
};

}  // namespace ssd1322_spi
}  // namespace esphome
