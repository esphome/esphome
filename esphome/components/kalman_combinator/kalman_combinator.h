#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace kalman_combinator {
class KalmanCombinatorComponent : public Component, public sensor::Sensor {
 public:
  KalmanCombinatorComponent(float update_variance = 1.) : update_variance_(update_variance) {}

  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void setup() override;

  void loop() override {}

  void add_source(Sensor *sensor, std::function<float(float)> const &stddev);
  void add_source(Sensor *sensor, float stddev);
  void set_std_dev_sensor(Sensor *sensor) { this->std_dev_sensor_ = sensor; }

 private:
  void update_();

  void correct_(float value, float stddev);

  std::vector<std::pair<Sensor *, std::function<float(float)>>> sensors_;

  // Optional sensor for publishing the current error
  sensor::Sensor *std_dev_sensor_{nullptr};

  uint32_t last_update_{0};
  float update_variance_{0.};
  float state_{NAN};
  float variance_{INFINITY};
};
}  // namespace kalman_combinator
}  // namespace esphome
