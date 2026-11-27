#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::derivative {

class DerivativeSensorBase : public sensor::Sensor, public Component {
 public:
  void dump_config() override;

 protected:
  void process_sensor_value_(float value, float ms_per_unit);

  uint32_t last_update_{0};
  float last_value_{NAN};  // NAN: no baseline yet
};

/// MS_PER_UNIT is the configured time unit in milliseconds, a constant rather than a member.
template<uint32_t MS_PER_UNIT> class DerivativeSensor final : public DerivativeSensorBase {
 public:
  explicit DerivativeSensor(sensor::Sensor *source) {
    source->add_on_state_callback([this](float state) { this->process_sensor_value_(state, MS_PER_UNIT); });
  }
};

}  // namespace esphome::derivative
