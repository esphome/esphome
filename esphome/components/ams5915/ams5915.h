#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ams5915 {

enum Transducer {
  AMS5915_0005_D,
  AMS5915_0010_D,
  AMS5915_0005_D_B,
  AMS5915_0010_D_B,
  AMS5915_0020_D,
  AMS5915_0050_D,
  AMS5915_0100_D,
  AMS5915_0020_D_B,
  AMS5915_0050_D_B,
  AMS5915_0100_D_B,
  AMS5915_0200_D,
  AMS5915_0350_D,
  AMS5915_1000_D,
  AMS5915_2000_D,
  AMS5915_4000_D,
  AMS5915_7000_D,
  AMS5915_10000_D,
  AMS5915_0200_D_B,
  AMS5915_0350_D_B,
  AMS5915_1000_D_B,
  AMS5915_1000_A,
  AMS5915_1200_B
};

class Ams5915 : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }
  void set_transducer_type(Transducer model);

  // transducer
 protected:
  Transducer type_;

  // buffer for I2C data
  uint8_t buffer_[4];

  // maximum number of attempts to talk to sensor
  static const size_t max_attempts_ = 10;
  // pressure digital output, counts
  uint16_t raw_pressure_data_;
  // temperature digital output, counts
  uint16_t raw_temperature_data_;
  // min and max pressure, millibar
  int p_min_;
  int p_max_;
  // conversion millibar to PA
  static const float mbar_to_pa_ = 100.0f;
  // digital output at minimum pressure
  static const int dig_out_p_min_ = 1638;
  // digital output at maximum pressure
  static const int dig_out_p_max_ = 14745;
  // min and max pressures, millibar
  static const int ams5915_0005_d_p_min_ = 0;
  static const int ams5915_0005_d_p_max_ = 5;
  static const int ams5915_0010_d_p_min_ = 0;
  static const int ams5915_0010_d_p_max_ = 10;
  static const int ams5915_0005_d_b_p_min_ = -5;
  static const int ams5915_0005_d_b_p_max_ = 5;
  static const int ams5915_0010_d_b_p_min_ = -10;
  static const int ams5915_0010_d_b_p_max_ = 10;
  static const int ams5915_0020_d_p_min_ = 0;
  static const int ams5915_0020_d_p_max_ = 20;
  static const int ams5915_0050_d_p_min_ = 0;
  static const int ams5915_0050_d_p_max_ = 50;
  static const int ams5915_0100_d_p_min_ = 0;
  static const int ams5915_0100_d_p_max_ = 100;
  static const int ams5915_0020_d_b_p_min_ = -20;
  static const int ams5915_0020_d_b_p_max_ = 20;
  static const int ams5915_0050_d_b_p_min_ = -50;
  static const int ams5915_0050_d_b_p_max_ = 50;
  static const int ams5915_0100_d_b_p_min_ = -100;
  static const int ams5915_0100_d_b_p_max_ = 100;
  static const int ams5915_0200_d_p_min_ = 0;
  static const int ams5915_0200_d_p_max_ = 200;
  static const int ams5915_0350_d_p_min_ = 0;
  static const int ams5915_0350_d_p_max_ = 350;
  static const int ams5915_1000_d_p_min_ = 0;
  static const int ams5915_1000_d_p_max_ = 1000;
  static const int ams5915_2000_d_p_min_ = 0;
  static const int ams5915_2000_d_p_max_ = 2000;
  static const int ams5915_4000_d_p_min_ = 0;
  static const int ams5915_4000_d_p_max_ = 4000;
  static const int ams5915_7000_d_p_min_ = 0;
  static const int ams5915_7000_d_p_max_ = 7000;
  static const int ams5915_10000_d_p_min_ = 0;
  static const int ams5915_10000_d_p_max_ = 10000;
  static const int ams5915_0200_d_b_p_min_ = -200;
  static const int ams5915_0200_d_b_p_max_ = 200;
  static const int ams5915_0350_d_b_p_min_ = -350;
  static const int ams5915_0350_d_b_p_max_ = 350;
  static const int ams5915_1000_d_b_p_min_ = -1000;
  static const int ams5915_1000_d_b_p_max_ = 1000;
  static const int ams5915_1000_a_p_min_ = 0;
  static const int ams5915_1000_a_p_max_ = 1000;
  static const int ams5915_1200_b_p_min_ = 700;
  static const int ams5915_1200_b_p_max_ = 1200;
  void get_transducer_();
  bool read_raw_data_(uint16_t *pressure_counts, uint16_t *temperature_counts);
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace ams5915
}  // namespace esphome
