#include "combination.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <cmath>

namespace esphome::combination {

static const char *const TAG = "combination";

void CombinationComponent::log_config_(const LogString *combo_type) {
  LOG_SENSOR("", "Combination Sensor:", this);
  ESP_LOGCONFIG(TAG, "  Combination Type: %s", LOG_STR_ARG(combo_type));
  this->log_source_sensors();
}

void CombinationNoParameterComponent::add_source(Sensor *sensor) { this->sensors_.emplace_back(sensor); }

void CombinationOneParameterComponent::add_source(Sensor *sensor, std::function<float(float)> const &compute) {
  this->sensor_sources_.push_back({sensor, compute, this});
}

void CombinationOneParameterComponent::add_source(Sensor *sensor, float value) {
  this->add_source(sensor, std::function<float(float)>{[value](float x) -> float { return value; }});
}

void CombinationNoParameterComponent::log_source_sensors() {
  ESP_LOGCONFIG(TAG, "  Source Sensors:");
  for (const auto &sensor : this->sensors_) {
    ESP_LOGCONFIG(TAG, "    - %s", sensor->get_name().c_str());
  }
}

void CombinationOneParameterComponent::log_source_sensors() {
  ESP_LOGCONFIG(TAG, "  Source Sensors:");
  for (const auto &source : this->sensor_sources_) {
    ESP_LOGCONFIG(TAG, "    - %s", source.sensor->get_name().c_str());
  }
}

void CombinationNoParameterComponent::setup() {
  for (const auto &sensor : this->sensors_) {
    // All sensor updates are deferred until the next loop. This avoids publishing the combined sensor's result
    // repeatedly in the same loop if multiple source senors update.
    sensor->add_on_state_callback(
        [this](float value) -> void { this->defer("update", [this, value]() { this->handle_new_value(value); }); });
  }
}

void KalmanCombinationComponent::dump_config() {
  this->log_config_(LOG_STR("kalman"));
  ESP_LOGCONFIG(TAG, "  Update variance: %f per ms", this->update_variance_value_);

  if (this->std_dev_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Standard Deviation Sensor:", this->std_dev_sensor_);
  }
}

void KalmanCombinationComponent::setup() {
  for (auto &source : this->sensor_sources_) {
    // [&source] is safe: source refers to a FixedVector element that never reallocates,
    // so the reference remains valid for the component's lifetime.
    source.sensor->add_on_state_callback([&source](float x) -> void {
      static_cast<KalmanCombinationComponent *>(source.parent)->correct_(x, source.compute(x));
    });
  }
}

void KalmanCombinationComponent::update_variance_() {
  uint32_t now = millis();

  // Variance increases by update_variance_ each millisecond
  auto dt = now - this->last_update_;
  auto dv = this->update_variance_value_ * dt;
  this->variance_ += dv;
  this->last_update_ = now;
}

void KalmanCombinationComponent::correct_(float value, float stddev) {
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

void LinearCombinationComponent::setup() {
  for (auto &source : this->sensor_sources_) {
    // All sensor updates are deferred until the next loop. This avoids publishing the combined sensor's result
    // repeatedly in the same loop if multiple source senors update.
    source.sensor->add_on_state_callback(
        [this](float value) -> void { this->defer("update", [this, value]() { this->handle_new_value(value); }); });
  }
}

void LinearCombinationComponent::handle_new_value(float value) {
  // Multiplies each sensor state by a configured coefficient and then sums

  if (!std::isfinite(value))
    return;

  float sum = 0.0;

  for (const auto &source : this->sensor_sources_) {
    const float sensor_state = source.sensor->state;
    if (std::isfinite(sensor_state)) {
      sum += sensor_state * source.compute(sensor_state);
    }
  }

  this->publish_state(sum);
};

void MaximumCombinationComponent::handle_new_value(float value) {
  if (!std::isfinite(value))
    return;

  float max_value = (-1) * std::numeric_limits<float>::infinity();  // note x = max(x, -infinity)

  for (const auto &sensor : this->sensors_) {
    if (std::isfinite(sensor->state)) {
      max_value = std::max(max_value, sensor->state);
    }
  }

  this->publish_state(max_value);
}

void MeanCombinationComponent::handle_new_value(float value) {
  if (!std::isfinite(value))
    return;

  float sum = 0.0;
  size_t count = 0;

  for (const auto &sensor : this->sensors_) {
    if (std::isfinite(sensor->state)) {
      ++count;
      sum += sensor->state;
    }
  }

  if (count == 0) {
    this->publish_state(NAN);
    return;
  }
  float mean = sum / count;

  this->publish_state(mean);
}

void MedianCombinationComponent::handle_new_value(float value) {
  // Sorts sensor states in ascending order and determines the middle value

  if (!std::isfinite(value))
    return;

  std::vector<float> sensor_states;
  for (const auto &sensor : this->sensors_) {
    if (std::isfinite(sensor->state)) {
      sensor_states.push_back(sensor->state);
    }
  }

  sort(sensor_states.begin(), sensor_states.end());
  size_t sensor_states_size = sensor_states.size();

  float median = NAN;

  if (sensor_states_size) {
    if (sensor_states_size % 2) {
      // Odd number of measurements, use middle measurement
      median = sensor_states[sensor_states_size / 2];
    } else {
      // Even number of measurements, use the average of the two middle measurements
      median = (sensor_states[sensor_states_size / 2] + sensor_states[sensor_states_size / 2 - 1]) / 2.0;
    }
  }

  this->publish_state(median);
}

void MinimumCombinationComponent::handle_new_value(float value) {
  if (!std::isfinite(value))
    return;

  float min_value = std::numeric_limits<float>::infinity();  // note x = min(x, infinity)

  for (const auto &sensor : this->sensors_) {
    if (std::isfinite(sensor->state)) {
      min_value = std::min(min_value, sensor->state);
    }
  }

  this->publish_state(min_value);
}

void MostRecentCombinationComponent::handle_new_value(float value) { this->publish_state(value); }

void RangeCombinationComponent::handle_new_value(float value) {
  // Sorts sensor states then takes difference between largest and smallest states

  if (!std::isfinite(value))
    return;

  std::vector<float> sensor_states;
  for (const auto &sensor : this->sensors_) {
    if (std::isfinite(sensor->state)) {
      sensor_states.push_back(sensor->state);
    }
  }

  if (sensor_states.empty()) {
    this->publish_state(NAN);
    return;
  }

  sort(sensor_states.begin(), sensor_states.end());

  float range = sensor_states.back() - sensor_states.front();
  this->publish_state(range);
}

void SumCombinationComponent::handle_new_value(float value) {
  if (!std::isfinite(value))
    return;

  float sum = 0.0;
  for (const auto &sensor : this->sensors_) {
    if (std::isfinite(sensor->state)) {
      sum += sensor->state;
    }
  }

  this->publish_state(sum);
}

}  // namespace esphome::combination
