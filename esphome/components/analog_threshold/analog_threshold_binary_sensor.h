#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace analog_threshold {

class AnalogThresholdBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void loop() override;
  void dump_config() override;
  void setup() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_sensor(sensor::Sensor *analog_sensor) { this->sensor_ = analog_sensor; }
  void set_upper_threshold(float threshold) { this->upper_threshold_ = threshold; }
  void set_lower_threshold(float threshold) { this->lower_threshold_ = threshold; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_delay_high(uint32_t delay) { this->delay_high_ = delay; }
  void set_delay_low(uint32_t delay) { this->delay_low_ = delay; }

  

 protected:

  bool is_above_() const;

  sensor::Sensor *sensor_{nullptr};
  bool inverted_{false};

  float upper_threshold_;
  float lower_threshold_;

  uint32_t delay_high_;
  uint32_t delay_low_;

  uint32_t last_change_{0};

  bool last_above_;
};

}  // namespace analog_threshold
}  // namespace esphome
