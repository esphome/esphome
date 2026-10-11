#pragma once

#include "esphome/components/sh1122_base/sh1122_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome::sh1122_spi {

class SPISH1122 final : public sh1122_base::SH1122,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_8MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }

  void setup() override;

  void dump_config() override;

 protected:
  void write_command_(const uint8_t *bytes, size_t len) override;
  void write_display_data() override;

  GPIOPin *dc_pin_{nullptr};
};

}  // namespace esphome::sh1122_spi
