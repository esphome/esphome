#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

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

class ADS1115Sensor;

class ADS1115Component : public Component, public i2c::I2CDevice {
 public:
  void register_sensor(ADS1115Sensor *obj) {
    this->sensors_.push_back(obj);
  }
  /// Set up the internal sensor array.
  void setup() override;
  void dump_config() override;
  /// HARDWARE_LATE setup priority
  float get_setup_priority() const override;

 protected:
  /// Helper method to request a measurement from a sensor.
  void request_measurement_(ADS1115Sensor *sensor);

  std::vector<ADS1115Sensor *> sensors_;
};

/// Internal holder class that is in instance of Sensor so that the hub can create individual sensors.
class ADS1115Sensor : public sensor::Sensor {
 public:
  ADS1115Sensor(const std::string &name, uint32_t update_interval)
      : sensor::Sensor(name), update_interval_(update_interval) {}

  void set_multiplexer(ADS1115Multiplexer multiplexer);
  void set_gain(ADS1115Gain gain);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  uint8_t get_multiplexer() const;
  uint8_t get_gain() const;

 protected:
  ADS1115Multiplexer multiplexer_;
  ADS1115Gain gain_;
  uint32_t update_interval_;
};

}  // namespace ads1115
}  // namespace esphome
