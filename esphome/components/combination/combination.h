#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace combination {

enum CombinationType {
  COMBINATION_KALMAN,
  COMBINATION_LINEAR,
  COMBINATION_MAXIMUM,
  COMBINATION_MEAN,
  COMBINATION_MEDIAN,
  COMBINATION_MINIMUM,
  COMBINATION_MOST_RECENTLY_UPDATED,
  COMBINATION_RANGE,
};

class CombinationComponent : public Component, public sensor::Sensor {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void dump_config() override;
  void setup() override;

  void add_source(Sensor *sensor, std::function<float(float)> const &stddev);
  void add_source(Sensor *sensor, float stddev);

  void set_combo_type(CombinationType combo_type) { this->combo_type_ = combo_type; }

  // Used only for Kalman combinator type
  void set_process_std_dev(float process_std_dev) {
    this->update_variance_value_ = process_std_dev * process_std_dev * 0.001f;
  }
  void set_std_dev_sensor(Sensor *sensor) { this->std_dev_sensor_ = sensor; }

 private:
  CombinationType combo_type_{};

  // Source sensors and their helper functions
  std::vector<std::pair<Sensor *, std::function<float(float)>>> sensors_;

  // Used for all types except Kalman combinator type
  void handle_new_value_(float value);

  // Used only for Kalman combinator type
  void update_variance_();
  void correct_(float value, float stddev);

  // Optional sensor for publishing the current error when using Kalman combinator type
  sensor::Sensor *std_dev_sensor_{nullptr};

  // Tick of the last update, used for Kalman combinator type
  uint32_t last_update_{0};
  // Change of the variance, per ms, used for Kalman combinator type
  float update_variance_value_{0.f};

  // Best guess for the state and its variance, used for Kalman combinator type
  float state_{NAN};
  float variance_{INFINITY};
};

}  // namespace combination
}  // namespace esphome
