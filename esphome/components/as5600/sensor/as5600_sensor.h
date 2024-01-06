#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/as5600/as5600.h"

namespace esphome {
namespace as5600 {

class AS5600Sensor : public PollingComponent, public Parented<AS5600Component>, public sensor::Sensor {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_angle_sensor(sensor::Sensor *angle_sensor) { this->angle_sensor_ = angle_sensor; }
  void set_raw_angle_sensor(sensor::Sensor *raw_angle_sensor) { this->raw_angle_sensor_ = raw_angle_sensor; }
  void set_position_sensor(sensor::Sensor *position_sensor) { this->position_sensor_ = position_sensor; }
  void set_raw_position_sensor(sensor::Sensor *raw_position_sensor) {
    this->raw_position_sensor_ = raw_position_sensor;
  }
  void set_gain_sensor(sensor::Sensor *gain_sensor) { this->gain_sensor_ = gain_sensor; }
  void set_magnitude_sensor(sensor::Sensor *magnitude_sensor) { this->magnitude_sensor_ = magnitude_sensor; }
  void set_status_sensor(sensor::Sensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_out_of_range_mode(OutRangeMode oor_mode) { this->out_of_range_mode_ = oor_mode; }
  OutRangeMode get_out_of_range_mode() { return this->out_of_range_mode_; }

 protected:
  sensor::Sensor *angle_sensor_{nullptr};
  sensor::Sensor *raw_angle_sensor_{nullptr};
  sensor::Sensor *position_sensor_{nullptr};
  sensor::Sensor *raw_position_sensor_{nullptr};
  sensor::Sensor *gain_sensor_{nullptr};
  sensor::Sensor *magnitude_sensor_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};
  OutRangeMode out_of_range_mode_{OUT_RANGE_MODE_MIN_MAX};
};

}  // namespace as5600
}  // namespace esphome
