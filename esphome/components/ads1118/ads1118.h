#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"

namespace esphome::ads1118 {

static constexpr uint16_t CONFIG_TEMPERATURE = 1 << 4;

class ADS1118 final : public Component,
                      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                            spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  /// Returns volts, or degrees Celsius when `config_bits` has CONFIG_TEMPERATURE.
  float request_measurement(uint16_t config_bits);
};

}  // namespace esphome::ads1118
