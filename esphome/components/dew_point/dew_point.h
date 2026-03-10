#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <cmath>

namespace esphome::dew_point {

class DewPointComponent : public Component, public sensor::Sensor {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void update_dew_point_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  float temperature_value_{NAN};
  float humidity_value_{NAN};
};

}  // namespace esphome::dew_point
