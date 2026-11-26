#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::derivative {

class DerivativeSensor final : public sensor::Sensor, public Component {
 public:
  explicit DerivativeSensor(sensor::Sensor *source) : source_(source) {}

  void setup() override;
  void dump_config() override;
  /// Milliseconds -> the configured time unit, worked out in code generation.
  void set_time_factor(float time_factor) { this->time_factor_ = time_factor; }

 protected:
  void process_sensor_value_(float value);

  sensor::Sensor *source_;
  float time_factor_;
  uint32_t last_update_{0};
  // No baseline until a valid reading arrives; a NaN last value marks "no baseline".
  float last_value_{NAN};
};

}  // namespace esphome::derivative
