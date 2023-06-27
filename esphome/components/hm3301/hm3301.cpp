#include "esphome/core/log.h"
#include "hm3301.h"

namespace esphome {
namespace hm3301 {

static const char *const TAG = "hm3301.sensor";

static const uint8_t PM_1_0_VALUE_INDEX = 5;
static const uint8_t PM_2_5_VALUE_INDEX = 6;
static const uint8_t PM_10_0_VALUE_INDEX = 7;

void HM3301Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HM3301...");
  if (i2c::ERROR_OK != this->write(&SELECT_COMM_CMD, 1)) {
    error_code_ = ERROR_COMM;
    this->mark_failed();
    return;
  }
}

void HM3301Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HM3301:");
  LOG_I2C_DEVICE(this);
  if (error_code_ == ERROR_COMM) {
    ESP_LOGE(TAG, "Communication with HM3301 failed!");
  }

  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "AQI", this->aqi_sensor_);
}

float HM3301Component::get_setup_priority() const { return setup_priority::DATA; }

void HM3301Component::update() {
  if (this->read(data_buffer_, 29) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Read result failed");
    this->status_set_warning();
    return;
  }

  if (!this->validate_checksum_(data_buffer_)) {
    ESP_LOGW(TAG, "Checksum validation failed");
    this->status_set_warning();
    return;
  }

  int16_t pm_1_0_value = -1;
  if (this->pm_1_0_sensor_ != nullptr) {
    pm_1_0_value = get_sensor_value_(data_buffer_, PM_1_0_VALUE_INDEX);
  }

  int16_t pm_2_5_value = -1;
  if (this->pm_2_5_sensor_ != nullptr) {
    pm_2_5_value = get_sensor_value_(data_buffer_, PM_2_5_VALUE_INDEX);
  }

  int16_t pm_10_0_value = -1;
  if (this->pm_10_0_sensor_ != nullptr) {
    pm_10_0_value = get_sensor_value_(data_buffer_, PM_10_0_VALUE_INDEX);
  }

  int16_t aqi_value = -1;
  if (this->aqi_sensor_ != nullptr && pm_2_5_value != -1 && pm_10_0_value != -1) {
    AbstractAQICalculator *calculator = this->aqi_calculator_factory_.get_calculator(this->aqi_calc_type_);
    aqi_value = calculator->get_aqi(pm_2_5_value, pm_10_0_value);
  }

  if (pm_1_0_value != -1) {
    this->pm_1_0_sensor_->publish_state(pm_1_0_value);
  }
  if (pm_2_5_value != -1) {
    this->pm_2_5_sensor_->publish_state(pm_2_5_value);
  }
  if (pm_10_0_value != -1) {
    this->pm_10_0_sensor_->publish_state(pm_10_0_value);
  }
  if (aqi_value != -1) {
    this->aqi_sensor_->publish_state(aqi_value);
  }

  this->status_clear_warning();
}

bool HM3301Component::validate_checksum_(const uint8_t *data) {
  uint8_t sum = 0;
  for (int i = 0; i < 28; i++) {
    sum += data[i];
  }

  return sum == data[28];
}

uint16_t HM3301Component::get_sensor_value_(const uint8_t *data, uint8_t i) {
  return (uint16_t) data[i * 2] << 8 | data[i * 2 + 1];
}

}  // namespace hm3301
}  // namespace esphome
