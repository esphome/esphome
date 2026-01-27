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
  if (this->capacitance_raw_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Raw Moisture Sensor:");
    LOG_SENSOR("", "Raw Moisture", this->capacitance_raw_);
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

  if (this->capacitance_raw_ != nullptr) {
    uint16_t raw;
    if (this->read_capacitance_(raw)) {
      float raw_f = static_cast<float>(raw);
      this->capacitance_raw_->publish_state(raw_f);
    }
  }
}

bool AdafruitSoilSensor::read_temp_c_(float &temp_c) {
  uint8_t buf[4];
  if (!this->read_(BaseAddress::STATUS, StatusAddress::TEMPERATURE, buf, 4)) {
    return false;
  }
  int32_t ret = ((uint32_t) buf[0] << 24) | ((uint32_t) buf[1] << 16) | ((uint32_t) buf[2] << 8) | (uint32_t) buf[3];
  temp_c = (1.0 / (1UL << 16)) * ret;
  return true;
}

bool AdafruitSoilSensor::read_capacitance_(uint16_t &touch_value) {
  uint8_t buf[2];
  uint8_t p = 0;  // only one channel
  for (uint8_t retry = 0; retry < 5; retry++) {
    if (this->read_(BaseAddress::TOUCH, TouchAddress::CHAN_0 + p, buf, 2)) {
      touch_value = ((uint16_t) buf[0] << 8) | buf[1];
      return true;
    }
  }
  return false;
}

bool AdafruitSoilSensor::read_(uint8_t reg_high, uint8_t reg_low, uint8_t *buf, uint16_t len) {
  uint8_t cmd[2] = {reg_high, reg_low};
  i2c::ErrorCode res = this->write(cmd, 2);
  if (res != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "Error starting i2c txn");
    return false;
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
