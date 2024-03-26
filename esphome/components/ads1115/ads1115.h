#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome {
namespace ads1115 {

enum ADS1115Multiplexer {
  ADS1115_MULTIPLEXER_P0_N1 = 0b000,
  ADS1115_MULTIPLEXER_P0_N3 = 0b001,
  ADS1115_MULTIPLEXER_P1_N3 = 0b010,
  ADS1115_MULTIPLEXER_P2_N3 = 0b011,
  ADS1115_MULTIPLEXER_P0_NG = 0b100,
  ADS1115_MULTIPLEXER_P1_NG = 0b101,
  ADS1115_MULTIPLEXER_P2_NG = 0b110,
  ADS1115_MULTIPLEXER_P3_NG = 0b111,
};

enum ADS1115Gain {
  ADS1115_GAIN_6P144 = 0b000,
  ADS1115_GAIN_4P096 = 0b001,
  ADS1115_GAIN_2P048 = 0b010,
  ADS1115_GAIN_1P024 = 0b011,
  ADS1115_GAIN_0P512 = 0b100,
  ADS1115_GAIN_0P256 = 0b101,
};

enum ADS1115Resolution {
  ADS1115_16_BITS = 16,
  ADS1015_12_BITS = 12,
};

class ADS1115Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// HARDWARE_LATE setup priority
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_continuous_mode(bool continuous_mode) { continuous_mode_ = continuous_mode; }

  /// Helper method to request a measurement from a sensor.
  float request_measurement(ADS1115Multiplexer multiplexer, ADS1115Gain gain, ADS1115Resolution resolution);

 protected:
  uint16_t prev_config_{0};
  bool continuous_mode_;
};

}  // namespace ads1115
}  // namespace esphome
