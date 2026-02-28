#include "aqi_sensor.h"
#include "esphome/core/log.h"

namespace esphome::aqi {

static const char *const TAG = "aqi";

void AQISensor::setup() {
  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->add_on_state_callback([this](float value) {
      this->pm_2_5_value_ = value;
      // Defer calculation to avoid double-publishing if both sensors update in the same loop
      this->defer("update", [this]() { this->calculate_aqi_(); });
    });
  }
  if (this->pm_10_0_sensor_ != nullptr) {
    this->pm_10_0_sensor_->add_on_state_callback([this](float value) {
      this->pm_10_0_value_ = value;
      this->defer("update", [this]() { this->calculate_aqi_(); });
    });
  }
}

void AQISensor::dump_config() {
  ESP_LOGCONFIG(TAG, "AQI Sensor:");
  ESP_LOGCONFIG(TAG, "  Calculation Type: %s", this->aqi_calc_type_ == AQI_TYPE ? "AQI" : "CAQI");
  if (this->pm_2_5_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  PM2.5 Sensor: '%s'", this->pm_2_5_sensor_->get_name().c_str());
  }
  if (this->pm_10_0_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  PM10 Sensor: '%s'", this->pm_10_0_sensor_->get_name().c_str());
  }
  LOG_SENSOR("  ", "AQI", this);
}

void AQISensor::calculate_aqi_() {
  if (std::isnan(this->pm_2_5_value_) || std::isnan(this->pm_10_0_value_)) {
    return;
  }

  AbstractAQICalculator *calculator = this->aqi_calculator_factory_.get_calculator(this->aqi_calc_type_);
  if (calculator == nullptr) {
    ESP_LOGW(TAG, "Unknown AQI calculator type");
    return;
  }

  uint16_t aqi = calculator->get_aqi(this->pm_2_5_value_, this->pm_10_0_value_);
  this->publish_state(aqi);
}

}  // namespace esphome::aqi
