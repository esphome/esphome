#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::exponential_moving_average {

/// Which value is assumed to apply during the time between two readings, when a time constant is used.
enum TimeWeighting : uint8_t {
  TIME_WEIGHTING_NEW = 0,
  TIME_WEIGHTING_PREVIOUS,
  TIME_WEIGHTING_LINEAR,
};

const LogString *time_weighting_to_string(TimeWeighting weighting);

/// A duration in the largest of ms, s, min or h that keeps the value at 1 or more.
struct ScaledDuration {
  float value;
  const LogString *unit;
  uint8_t decimals;
};

ScaledDuration scale_duration(uint32_t ms);

class ExponentialMovingAverageSensor : public sensor::Sensor, public Component {
 public:
  explicit ExponentialMovingAverageSensor(sensor::Sensor *source) : source_(source) {}

  void setup() override;
  void dump_config() override;

  void set_alpha(float alpha) { this->alpha_ = alpha; }
  /// When non-zero, each sample is weighted by the time since the previous one instead of by a fixed alpha.
  void set_time_constant(uint32_t time_constant_ms) { this->time_constant_ms_ = time_constant_ms; }
  void set_time_weighting(TimeWeighting weighting) { this->time_weighting_ = weighting; }
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
  float previous_value_{NAN};
  uint32_t time_constant_ms_{0};
  uint32_t last_update_{0};
  TimeWeighting time_weighting_{TIME_WEIGHTING_NEW};
  bool restore_{true};
};

}  // namespace esphome::exponential_moving_average
