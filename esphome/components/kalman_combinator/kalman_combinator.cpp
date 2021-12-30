#include "kalman_combinator.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <functional>

namespace esphome {
namespace kalman_combinator {

void KalmanCombinatorComponent::dump_config() {
  ESP_LOGCONFIG("kalman_combinator", "Kalman Combinator:");
  ESP_LOGCONFIG("kalman_combinator", "  Update variance per ms: %f", this->update_variance_);
  ESP_LOGCONFIG("kalman_combinator", "  Sensors:");
  for (auto sensor : this->sensors_) {
    auto &entity = *sensor.first;
    ESP_LOGCONFIG("kalman_combinator", "    - %s", entity.get_object_id().c_str());
  }
}

void KalmanCombinatorComponent::setup() {
  for (auto sensor : this->sensors_) {
    const auto stddev = sensor.second;
    sensor.first->add_on_state_callback([this, stddev](float x) -> void { this->correct_(x, stddev(x)); });
  }

  if (this->std_dev_sensor_) {
    auto &s = *this->std_dev_sensor_;
    if (s.get_unit_of_measurement() == std::string("")) {
      s.set_unit_of_measurement(this->get_unit_of_measurement());
    }
    if (s.get_device_class() == std::string("")) {
      s.set_device_class(this->get_device_class());
    }
  }
}

std::string KalmanCombinatorComponent::device_class() {
  return this->device_class_.value_or(this->sensors_.size() != 0 ? this->sensors_[0].first->get_device_class() : std::string(""));
}

std::string KalmanCombinatorComponent::unit_of_measurement() {
  return this->unit_of_measurement_.value_or(this->sensors_.size() != 0 ? this->sensors_[0].first->get_unit_of_measurement() : std::string(""));
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
    // Variance increases by update_variance_ each second
    auto dt = now - this->last_update_;
    auto dv = this->update_variance_ * dt;
    this->variance_ += dv;
    this->last_update_ = now;
  }
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

  // Update the variance
  this->update_();

  // Combine two gaussian distributions mu1+-var1, mu2+-var2 to a new one around mu
  // Use the value with the smaller variance as mu1 to prevent precision errors
  const bool this_first = this->variance_ < (stddev*stddev);
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
