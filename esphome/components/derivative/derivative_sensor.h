#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::derivative {

class DerivativeSensor final : public sensor::Sensor, public Component {
 public:
  /// time_factor converts milliseconds to the configured time unit.
  DerivativeSensor(sensor::Sensor *source, float time_factor) : time_factor_(time_factor) {
    source->add_on_state_callback([this](float state) { this->process_sensor_value_(state); });
  }

  void dump_config() override;

 protected:
  void process_sensor_value_(float value);

  float time_factor_;
  uint32_t last_update_{0};
  float last_value_{NAN};  // NAN: no baseline yet
};

}  // namespace esphome::derivative
