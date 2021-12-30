#include "kalman_combinator.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <functional>

namespace esphome {
namespace kalman_combinator {

void KalmanCombinatorComponent::setup() {
  for (auto sensor : this->sensors_) {
    const auto stddev = sensor.second;
    sensor.first->add_on_state_callback([this, stddev](float x) -> void { this->correct_(x, stddev(x)); });
  }
}

void KalmanCombinatorComponent::add_source(Sensor *sensor, std::function<float(float)> const &stddev) {
  this->sensors_.emplace_back(sensor, stddev);
}

void KalmanCombinatorComponent::add_source(Sensor *sensor, float stddev) {
  this->add_source(sensor, std::function<float(float)>{[stddev](float) -> float { return stddev; }});
}

void KalmanCombinatorComponent::update_() {
  uint32_t now = millis();
  if (now != this->last_update_) {
    auto dt = now - this->last_update_;
    auto dv = this->update_variance_ * dt * 0.001;
    this->variance_ += dv;
    this->last_update_ = now;
  }
}

void KalmanCombinatorComponent::correct_(float value, float stddev) {
  this->update_();

  if (std::isnan(this->state_) || std::isinf(this->variance_)) {
    this->state_ = value;
    this->variance_ = stddev * stddev;
    return;
  }

  if (std::isnan(value) || std::isinf(stddev)) {
    return;
  }

  const float mu1 = this->state_;
  const float mu2 = value;

  const float var1 = this->variance_;
  const float var2 = stddev * stddev;

  const float mu = mu1 + var1 * (mu2 - mu1) / (var1 + var2);
  const float var = var1 - (var1 * var1) / (var1 + var2);

  this->state_ = mu;
  this->variance_ = var;

  this->publish_state(mu);
}
}  // namespace kalman_combinator
}  // namespace esphome
