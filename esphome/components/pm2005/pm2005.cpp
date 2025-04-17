#include "esphome/core/log.h"
#include "pm2005.h"

namespace esphome {
namespace pm2005 {

static const char *const TAG = "pm2005";

// Converts a sensor situation to a human readable string
static const LogString *pm2005_get_situation_string(int status) {
  switch (status) {
    case 1:
      return LOG_STR("Close");
    case 2:
      return LOG_STR("Malfunction");
    case 3:
      return LOG_STR("Under detecting");
    case 0x80:
      return LOG_STR("Detecting completed");
    default:
      return LOG_STR("Invalid");
  }
}

// Converts a sensor measuring mode to a human readable string
static const LogString *pm2005_get_measuring_mode_string(int status) {
  switch (status) {
    case 2:
      return LOG_STR("Single");
    case 3:
      return LOG_STR("Continuous");
    case 5:
      return LOG_STR("Dynamic");
    default:
      return LOG_STR("Timing");
  }
}

static inline uint16_t get_sensor_value(const uint8_t *data, uint8_t i) { return data[i] * 0x100 + data[i + 1]; }

void PM2005Component::setup() {
  if (this->sensor_type_ == PM2005) {
    ESP_LOGCONFIG(TAG, "Setting up PM2005...");

    this->situation_value_index_ = 3;
    this->pm_1_0_value_index_ = 4;
    this->pm_2_5_value_index_ = 6;
    this->pm_10_0_value_index_ = 8;
    this->measuring_value_index_ = 10;
  } else {
    ESP_LOGCONFIG(TAG, "Setting up PM2105...");

    this->situation_value_index_ = 2;
    this->pm_1_0_value_index_ = 3;
    this->pm_2_5_value_index_ = 5;
    this->pm_10_0_value_index_ = 7;
    this->measuring_value_index_ = 9;
  }

  if (this->read(this->data_buffer_, 12) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication failed!");
    this->mark_failed();
    return;
  }
}

void PM2005Component::update() {
  if (this->read(this->data_buffer_, 12) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Read result failed.");
    this->status_set_warning();
    return;
  }

  if (this->sensor_situation_ == this->data_buffer_[this->situation_value_index_]) {
    return;
  }

  this->sensor_situation_ = this->data_buffer_[this->situation_value_index_];
  ESP_LOGD(TAG, "Sensor situation: %s.", LOG_STR_ARG(pm2005_get_situation_string(this->sensor_situation_)));
  if (this->sensor_situation_ == 2) {
    this->status_set_warning();
    return;
  }
  if (this->sensor_situation_ != 0x80) {
    return;
  }

  uint16_t pm1 = get_sensor_value(this->data_buffer_, this->pm_1_0_value_index_);
  uint16_t pm25 = get_sensor_value(this->data_buffer_, this->pm_2_5_value_index_);
  uint16_t pm10 = get_sensor_value(this->data_buffer_, this->pm_10_0_value_index_);
  uint16_t sensor_measuring_mode = get_sensor_value(this->data_buffer_, this->measuring_value_index_);
  ESP_LOGD(TAG, "PM1.0: %d, PM2.5: %d, PM10: %d, Measuring mode: %s.", pm1, pm25, pm10,
           LOG_STR_ARG(pm2005_get_measuring_mode_string(sensor_measuring_mode)));

  if (this->pm_1_0_sensor_ != nullptr) {
    this->pm_1_0_sensor_->publish_state(pm1);
  }
  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm25);
  }
  if (this->pm_10_0_sensor_ != nullptr) {
    this->pm_10_0_sensor_->publish_state(pm10);
  }

  this->status_clear_warning();
}

void PM2005Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PM2005:");
  ESP_LOGCONFIG(TAG, "  Type: PM2%u05", this->sensor_type_ == PM2105);

  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PM2%u05 failed!", this->sensor_type_ == PM2105);
  }

  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10 ", this->pm_10_0_sensor_);
}

}  // namespace pm2005
}  // namespace esphome
