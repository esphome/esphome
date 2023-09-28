#include "combination.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <functional>
#include <vector>

namespace esphome {
namespace combination {

static const char *const TAG = "combination";

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
static const LogString *combination_type_to_str(CombinationType type) {
  switch (type) {
    case CombinationType::COMBINATION_KALMAN:
      return LOG_STR("kalman");
    case CombinationType::COMBINATION_LINEAR:
      return LOG_STR("linear");
    case CombinationType::COMBINATION_MAXIMUM:
      return LOG_STR("maximum");
    case CombinationType::COMBINATION_MEAN:
      return LOG_STR("mean");
    case CombinationType::COMBINATION_MEDIAN:
      return LOG_STR("median");
    case CombinationType::COMBINATION_MINIMUM:
      return LOG_STR("minimum");
    case CombinationType::COMBINATION_MOST_RECENTLY_UPDATED:
      return LOG_STR("most recently updated");
    case CombinationType::COMBINATION_RANGE:
      return LOG_STR("range");
    default:
      return LOG_STR("");
  }
}
#endif

void CombinationComponent::dump_config() {
  LOG_SENSOR("", "Combination Sensor:", this);
  ESP_LOGCONFIG(TAG, "  Type: %s", LOG_STR_ARG(combination_type_to_str(this->combo_type_)));

  if (this->combo_type_ == CombinationType::COMBINATION_KALMAN) {
    ESP_LOGCONFIG(TAG, "  Update variance: %f per ms", this->update_variance_value_);
  }

  if (this->std_dev_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Standard Deviation Sensor:", this->std_dev_sensor_);
  }

  ESP_LOGCONFIG(TAG, "  Source Sensors:");
  for (const auto &sensor : this->sensors_) {
    auto &entity = *sensor.first;
    ESP_LOGCONFIG(TAG, "    - %s", entity.get_name().c_str());
  }
}

void CombinationComponent::setup() {
  for (const auto &sensor : this->sensors_) {
    if (this->combo_type_ == CombinationType::COMBINATION_KALMAN) {
      const auto stddev = sensor.second;
      sensor.first->add_on_state_callback([this, stddev](float x) -> void { this->correct_(x, stddev(x)); });
    } else {
      sensor.first->add_on_state_callback([this](float value) -> void { this->handle_new_value_(value); });
    }
  }
}

void CombinationComponent::loop() {
  // Any updates are first stored in updated_value_ and then sent in the next loop. This prevents the combination sensor
  // from publishing multiple times when more than one sensor is updated in the same loop.
  if (std::isfinite(this->updated_value_)) {
    this->publish_state(this->updated_value_);
    this->updated_value_ = NAN;
  }
}

void CombinationComponent::handle_new_value_(float value) {
  if (std::isfinite(value)) {
    switch (this->combo_type_) {
      case CombinationType::COMBINATION_LINEAR: {
        // Multiplies sensor states by a configured constant and then sums them

        float sum = 0.0;

        for (const auto &sensor : this->sensors_) {
          if (std::isfinite(sensor.first->state)) {
            sum += sensor.first->state * sensor.second(0);
          }
        }

        this->updated_value_ = sum;
        break;
      }
      case CombinationType::COMBINATION_MAXIMUM: {
        float max_value =
            (-1) * std::numeric_limits<float>::infinity();  // The max of a number with -infinity is that number

        for (const auto &sensor : this->sensors_) {
          max_value = std::max(max_value, sensor.first->state);
        }

        this->updated_value_ = max_value;
        break;
      }
      case CombinationType::COMBINATION_MEAN: {
        float sum = 0.0;
        size_t count = 0.0;

        for (const auto &sensor : this->sensors_) {
          if (std::isfinite(sensor.first->state)) {
            ++count;
            sum += sensor.first->state;
          }
        }

        this->updated_value_ = sum / count;
        break;
      }
      case CombinationType::COMBINATION_MEDIAN: {
        // Sorts sensor states in ascending order
        std::vector<float> sensor_states;
        for (const auto &sensor : this->sensors_) {
          if (std::isfinite(sensor.first->state)) {
            sensor_states.push_back(sensor.first->state);
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

        this->updated_value_ = median;
        break;
      }
      case CombinationType::COMBINATION_MINIMUM: {
        float min_value = std::numeric_limits<float>::infinity();  // The min of a number with infinity is that number

        for (const auto &sensor : this->sensors_) {
          min_value = std::min(min_value, sensor.first->state);
        }

        this->updated_value_ = min_value;
        break;
      }
      case CombinationType::COMBINATION_MOST_RECENTLY_UPDATED: {
        this->updated_value_ = value;
        break;
      }
      case CombinationType::COMBINATION_RANGE: {
        // Sorts sensor states then takes difference between largest and smallest states

        std::vector<float> sensor_states;
        for (const auto &sensor : this->sensors_) {
          if (std::isfinite(sensor.first->state)) {
            sensor_states.push_back(sensor.first->state);
          }
        }

        sort(sensor_states.begin(), sensor_states.end());

        this->updated_value_ = sensor_states.back() - sensor_states.front();
        break;
      }
      default:
        return;
    }
  }
}

void CombinationComponent::add_source(Sensor *sensor, std::function<float(float)> const &stddev) {
  this->sensors_.emplace_back(sensor, stddev);
}

void CombinationComponent::add_source(Sensor *sensor, float stddev) {
  this->add_source(sensor, std::function<float(float)>{[stddev](float x) -> float { return stddev; }});
}

void CombinationComponent::update_variance_() {
  uint32_t now = millis();

  // Variance increases by update_variance_ each millisecond
  auto dt = now - this->last_update_;
  auto dv = this->update_variance_value_ * dt;
  this->variance_ += dv;
  this->last_update_ = now;
}

void CombinationComponent::correct_(float value, float stddev) {
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

}  // namespace combination
}  // namespace esphome
