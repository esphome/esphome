#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::exponential_moving_average {

class ExponentialMovingAverageSensor : public sensor::Sensor, public Component {
 public:
  explicit ExponentialMovingAverageSensor(sensor::Sensor *source) : source_(source) {}

  void setup() override;
  void dump_config() override;

  void set_alpha(float alpha) { this->alpha_ = alpha; }
  /// When non-zero, each sample is weighted by the time since the previous one instead of by a fixed alpha.
  void set_time_constant(uint32_t time_constant_ms) { this->time_constant_ms_ = time_constant_ms; }
  void set_restore(bool restore) { this->restore_ = restore; }
  /// Clear the average; the next sample starts it again.
  void reset();

 protected:
  void process_(float value, uint32_t now);
  void publish_and_save_(float value);

  sensor::Sensor *source_;
  ESPPreferenceObject pref_;
  float alpha_{0.1f};
  float accumulator_{NAN};
  uint32_t time_constant_ms_{0};
  uint32_t last_update_{0};
  bool restore_{true};
};

}  // namespace esphome::exponential_moving_average
