#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace kalman_combinator {
class KalmanCombinatorComponent : public Component, public sensor::Sensor {
 public:
  KalmanCombinatorComponent(float update_variance = 1.) : update_variance(update_variance) {}

  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void setup() override;

  void loop() override {}

  void add_source(Sensor *sensor, std::function<float(float)> const &stddev);
  void add_source(Sensor *sensor, float stddev);

 private:
  void update();

  void correct(float value, float stddev);

  std::vector<std::pair<Sensor *, std::function<float(float)>>> sensors;

  uint32_t last_update{0};
  float update_variance{0.};
  float state{NAN};
  float variance{INFINITY};
};
}  // namespace kalman_combinator
}  // namespace esphome
