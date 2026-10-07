
#include "dew_point.h"

namespace esphome::dew_point {

static const char *const TAG = "dew_point.sensor";

void DewPointComponent::setup() {
  // Register callbacks for sensor updates
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->add_on_state_callback([this](float state) {
      this->temperature_value_ = state;
      this->enable_loop();
    });
    // Get initial value
    if (this->temperature_sensor_->has_state()) {
      this->temperature_value_ = this->temperature_sensor_->get_state();
    }
  }

  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->add_on_state_callback([this](float state) {
      this->humidity_value_ = state;
      this->enable_loop();
    });
    // Get initial value
    if (this->humidity_sensor_->has_state()) {
      this->humidity_value_ = this->humidity_sensor_->get_state();
    }
  }
}

void DewPointComponent::dump_config() {
  LOG_SENSOR("", "Dew Point", this);
  ESP_LOGCONFIG(TAG,
                "Sources\n"
                "  Temperature: '%s'\n"
                "  Humidity: '%s'",
                this->temperature_sensor_->get_name().c_str(), this->humidity_sensor_->get_name().c_str());
}

float DewPointComponent::get_setup_priority() const { return setup_priority::DATA; }

void DewPointComponent::loop() {
  // Only run once
  this->disable_loop();

  // Check if we have valid values for both sensors
  if (std::isnan(this->temperature_value_) || std::isnan(this->humidity_value_)) {
    ESP_LOGW(TAG, "Temperature or humidity value is NaN, skipping calculation");
    this->publish_state(NAN);
    return;
  }

  // Check for valid humidity range
  if (this->humidity_value_ <= 0.0f || this->humidity_value_ > 100.0f) {
    ESP_LOGW(TAG, "Humidity value out of range (0-100): %.2f", this->humidity_value_);
    this->publish_state(NAN);
    return;
  }

  // Magnus formula constants
  const float a{17.625f};
  const float b{243.04f};

  // Calculate dew point using Magnus formula
  // Td = (b * alpha) / (a - alpha)
  // where alpha = ln(RH/100) + (a * T) / (b + T)

  const float alpha{std::log(this->humidity_value_ / 100.0f) +
                    (a * this->temperature_value_) / (b + this->temperature_value_)};

  const float dew_point{(b * alpha) / (a - alpha)};

  // Publish the calculated dew point
  this->publish_state(dew_point);

  ESP_LOGD(TAG, "'%s' >> %.1f°C (T: %.1f°C, RH: %.1f%%)", this->get_name().c_str(), dew_point, this->temperature_value_,
           this->humidity_value_);
}

}  // namespace esphome::dew_point
