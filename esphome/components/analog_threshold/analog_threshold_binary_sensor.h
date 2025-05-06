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
  template<typename T> void set_upper_threshold(T upper_threshold) { this->upper_threshold_ = upper_threshold; }
  template<typename T> void set_lower_threshold(T lower_threshold) { this->lower_threshold_ = lower_threshold; }

 protected:
  sensor::Sensor *sensor_{nullptr};
  TemplatableValue<float> upper_threshold_{};
  TemplatableValue<float> lower_threshold_{};
};

}  // namespace analog_threshold
}  // namespace esphome
