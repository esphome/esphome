#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "adafruit_soil_sensor.h"

namespace esphome {
namespace adafruit_soil_sensor {

static const char *const TAG = "adafruit_soil_sensor";

void AdafruitSoilSensor::setup() { ESP_LOGCONFIG(TAG, "Setting up Adafruit seesaw soil sensor..."); }

void AdafruitSoilSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Adafruit seesaw soil sensor:");
  LOG_I2C_DEVICE(this);
  if (this->temperature_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature Sensor:");
    LOG_SENSOR("", "Temperature", this->temperature_);
  }
  if (this->moisture_raw_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Raw Moisture Sensor:");
    LOG_SENSOR("", "Raw Moisture", this->moisture_raw_);
  }
  if (this->moisture_raw_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Calibrated Moisture Sensor:");
    LOG_SENSOR("", "Calibrated Moisture", this->moisture_calibrated_);
  }
}

void AdafruitSoilSensor::update() {
  ESP_LOGD(TAG, "Updating sensor");
  if (this->is_failed())
    return;

  if (this->temperature_ != nullptr) {
    float c;
    if (this->read_temp_c_(c)) {
      this->temperature_->publish_state(c);
    } else {
      ESP_LOGW(TAG, "Failed to read temperature");
    }
  }

  if (this->moisture_raw_ != nullptr) {
    uint16_t raw;
    if (this->read_touch_(raw)) {
      float raw_f = static_cast<float>(raw);
      this->moisture_raw_->publish_state(raw_f);
      if (this->moisture_calibrated_ != nullptr) {
        if (dry_ == wet_) {
          ESP_LOGE(TAG, "Dry and wet calibration values are equal");
        }
        if (dry_ != 0 && wet_ != 0) {
          float moisture_percent = ((raw_f - dry_) / (wet_ - dry_)) * 100;
          if (moisture_percent > 100) {
            ESP_LOGD(TAG, "Calibrated value %f greater than 100%, clamping.", moisture_percent);
            moisture_percent = 100;
          }
          this->moisture_calibrated_->publish_state(moisture_percent);
        } else {
          ESP_LOGW(TAG, "Calibration sensor is configured but both values are 0");
        }
      }
    }
  }
}

bool AdafruitSoilSensor::read_temp_c_(float &temp_c) {
  uint8_t buf[4];
  if (!this->read_(BaseAddress::STATUS, StatusAddress::TEMPERATURE, buf, 4, 100)) {
    return false;
  }
  int32_t ret = ((uint32_t) buf[0] << 24) | ((uint32_t) buf[1] << 16) | ((uint32_t) buf[2] << 8) | (uint32_t) buf[3];
  temp_c = (1.0 / (1UL << 16)) * ret;
  return true;
}

bool AdafruitSoilSensor::read_touch_(uint16_t &touch_value) {
  uint8_t buf[2];
  uint8_t p = 0;  // only one channel
  for (uint8_t retry = 0; retry < 5; retry++) {
    if (this->read_(BaseAddress::TOUCH, TouchAddress::CHAN_0 + p, buf, 2, 300 + retry * 100)) {
      touch_value = ((uint16_t) buf[0] << 8) | buf[1];
      return true;
    }
  }
  return false;
}

bool AdafruitSoilSensor::read_(uint8_t reg_high, uint8_t reg_low, uint8_t *buf, uint16_t len, uint16_t delay_ms) {
  uint8_t cmd[2] = {reg_high, reg_low};
  i2c::ErrorCode res = this->write(cmd, 2);
  if (res != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "Error starting i2c txn");
    return false;
  }
  if (delay_ms > 0) {
    delay(delay_ms);
  }
  res = this->read(buf, len);
  if (res != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "Got error %d from read", res);
    return false;
  }
  return true;
}

}  // namespace adafruit_soil_sensor
}  // namespace esphome
