#include "kalman_combinator.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <functional>

namespace esphome {
namespace kalman_combinator {

void KalmanCombinatorComponent::dump_config() {
  ESP_LOGCONFIG("kalman_combinator", "Kalman Combinator:");
  ESP_LOGCONFIG("kalman_combinator", "  Update variance: %f per ms", this->update_variance_value_);
  ESP_LOGCONFIG("kalman_combinator", "  Sensors:");
  for (const auto &sensor : this->sensors_) {
    auto &entity = *sensor.first;
    ESP_LOGCONFIG("kalman_combinator", "    - %s", entity.get_name().c_str());
  }
}

void KalmanCombinatorComponent::setup() {
  for (const auto &sensor : this->sensors_) {
    const auto stddev = sensor.second;
    sensor.first->add_on_state_callback([this, stddev](float x) -> void { this->correct_(x, stddev(x)); });
  }
}

void KalmanCombinatorComponent::add_source(Sensor *sensor, std::function<float(float)> const &stddev) {
  this->sensors_.emplace_back(sensor, stddev);
}

void KalmanCombinatorComponent::add_source(Sensor *sensor, float stddev) {
  this->add_source(sensor, std::function<float(float)>{[stddev](float x) -> float { return stddev; }});
}

void KalmanCombinatorComponent::update_variance_() {
  uint32_t now = millis();

  // Variance increases by update_variance_ each millisecond
  auto dt = now - this->last_update_;
  auto dv = this->update_variance_value_ * dt;
  this->variance_ += dv;
  this->last_update_ = now;
}

void KalmanCombinatorComponent::correct_(float value, float stddev) {
  if (std::isnan(value) || std::isinf(stddev)) {
    return;
  }

  if (std::isnan(this->state_) || std::isinf(this->variance_)) {
    this->state_ = value;
    this->variance_ = stddev * stddev;
    if (this->std_dev_sensor_ != nullptr) {
      this->std_dev_sensor_->publish_state(stddev);
    }
    return;
  }

  this->update_variance_();

  // Combine two gaussian distributions mu1+-var1, mu2+-var2 to a new one around mu
  // Use the value with the smaller variance as mu1 to prevent precision errors
  const bool this_first = this->variance_ < (stddev * stddev);
  const float mu1 = this_first ? this->state_ : value;
  const float mu2 = this_first ? value : this->state_;

  const float var1 = this_first ? this->variance_ : stddev * stddev;
  const float var2 = this_first ? stddev * stddev : this->variance_;

  const float mu = mu1 + var1 * (mu2 - mu1) / (var1 + var2);
  const float var = var1 - (var1 * var1) / (var1 + var2);

  // Update and publish state
  this->state_ = mu;
  this->variance_ = var;

  this->publish_state(mu);
  if (this->std_dev_sensor_ != nullptr) {
    this->std_dev_sensor_->publish_state(std::sqrt(var));
  }
}
}  // namespace kalman_combinator
}  // namespace esphome
