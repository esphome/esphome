#include "kalman_combinator.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <functional>

namespace esphome {
namespace kalman_combinator {

void KalmanCombinatorComponent::setup() {
  for (auto sensor : this->sensors) {
    const auto stddev = sensor.second;
    sensor.first->add_on_state_callback([this, stddev](float x) -> void { this->correct(x, stddev(x)); });
  }
}

void KalmanCombinatorComponent::add_source(Sensor *sensor, std::function<float(float)> const &stddev) {
  this->sensors.push_back(std::make_pair(sensor, stddev));
}

void KalmanCombinatorComponent::add_source(Sensor *sensor, float stddev) {
  this->add_source(sensor, std::function<float(float)>{[stddev](float) -> float { return stddev; }});
}

void KalmanCombinatorComponent::update() {
  uint32_t now = millis();
  if (now != this->last_update) {
    auto dt = now - this->last_update;
    auto dv = this->update_variance * dt * 0.001;
    this->variance += dv;
    this->last_update = now;
  }
}

void KalmanCombinatorComponent::correct(float value, float stddev) {
  this->update();

  if (std::isnan(this->state) || std::isinf(this->variance)) {
    this->state = value;
    this->variance = stddev * stddev;
    return;
  }

  if (std::isnan(value) || std::isinf(stddev)) {
    return;
  }

  const float mu1 = this->state;
  const float mu2 = value;

  const float var1 = this->variance;
  const float var2 = stddev * stddev;

  const float mu = mu1 + var1 * (mu2 - mu1) / (var1 + var2);
  const float var = var1 - (var1 * var1) / (var1 + var2);

  this->state = mu;
  this->variance = var;

  this->publish_state(mu);
}
}  // namespace kalman_combinator
}  // namespace esphome
