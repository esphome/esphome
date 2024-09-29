#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ams5915 {

class Ams5915 : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }
  void set_pressure_range(int min_pressure, int max_pressure) {
    this->p_min_ = min_pressure;
    this->p_max_ = max_pressure;
  }

  // transducer
 protected:
  // buffer for I2C data
  uint8_t buffer_[4];

  // maximum number of attempts to talk to sensor
  static const size_t MAX_ATTEMPTS = 10;
  // pressure digital output, counts
  uint16_t raw_pressure_data_;
  // temperature digital output, counts
  uint16_t raw_temperature_data_;
  // min and max pressure, millibar
  int p_min_;
  int p_max_;
  // conversion millibar to PA
  static constexpr float MBAR_TO_PA = 100.0f;
  // digital output at minimum pressure
  static const int DIG_OUT_P_MIN = 1638;
  // digital output at maximum pressure
  static const int DIG_PUT_P_MAX = 14745;

  bool read_raw_data_(uint16_t *pressure_counts, uint16_t *temperature_counts);
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace ams5915
}  // namespace esphome
