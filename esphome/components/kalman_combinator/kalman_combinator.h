#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace kalman_combinator {

class KalmanCombinatorComponent : public Component, public sensor::Sensor {
 public:
  KalmanCombinatorComponent() = default;

  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void dump_config() override;
  void setup() override;

  void add_source(Sensor *sensor, std::function<float(float)> const &stddev);
  void add_source(Sensor *sensor, float stddev);
  void set_process_std_dev(float process_std_dev) {
    this->update_variance_value_ = process_std_dev * process_std_dev * 0.001f;
  }
  void set_std_dev_sensor(Sensor *sensor) { this->std_dev_sensor_ = sensor; }

 private:
  void update_variance_();
  void correct_(float value, float stddev);

  // Source sensors and their error functions
  std::vector<std::pair<Sensor *, std::function<float(float)>>> sensors_;

  // Optional sensor for publishing the current error
  sensor::Sensor *std_dev_sensor_{nullptr};

  // Tick of the last update
  uint32_t last_update_{0};
  // Change of the variance, per ms
  float update_variance_value_{0.f};

  // Best guess for the state and its variance
  float state_{NAN};
  float variance_{INFINITY};
};
}  // namespace kalman_combinator
}  // namespace esphome
