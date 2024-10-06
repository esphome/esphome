#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ads1118 {

enum ADS1118Multiplexer {
  ADS1118_MULTIPLEXER_P0_N1 = 0b000,
  ADS1118_MULTIPLEXER_P0_N3 = 0b001,
  ADS1118_MULTIPLEXER_P1_N3 = 0b010,
  ADS1118_MULTIPLEXER_P2_N3 = 0b011,
  ADS1118_MULTIPLEXER_P0_NG = 0b100,
  ADS1118_MULTIPLEXER_P1_NG = 0b101,
  ADS1118_MULTIPLEXER_P2_NG = 0b110,
  ADS1118_MULTIPLEXER_P3_NG = 0b111,
};

enum ADS1118Gain {
  ADS1118_GAIN_6P144 = 0b000,
  ADS1118_GAIN_4P096 = 0b001,
  ADS1118_GAIN_2P048 = 0b010,
  ADS1118_GAIN_1P024 = 0b011,
  ADS1118_GAIN_0P512 = 0b100,
  ADS1118_GAIN_0P256 = 0b101,
};

class ADS1118 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  ADS1118() = default;
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  /// Helper method to request a measurement from a sensor.
  float request_measurement(ADS1118Multiplexer multiplexer, ADS1118Gain gain, bool temperature_mode);

 protected:
  uint16_t config_{0};
};

}  // namespace ads1118
}  // namespace esphome
