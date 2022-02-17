#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace analog_threshold {

class AnalogThresholdBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void dump_config() override;
  void setup() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sensor(sensor::Sensor *analog_sensor);
  void set_upper_threshold(float threshold) { this->upper_threshold_ = threshold; }
  void set_lower_threshold(float threshold) { this->lower_threshold_ = threshold; }

 protected:
  sensor::Sensor *sensor_{nullptr};

  float upper_threshold_;
  float lower_threshold_;
};

}  // namespace analog_threshold
}  // namespace esphome
