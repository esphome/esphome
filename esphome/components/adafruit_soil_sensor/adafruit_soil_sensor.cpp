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
  ESP_LOGCONFIG(TAG, "Read Delay: %dms", this->read_delay_);
}

void AdafruitSoilSensor::update() {
  ESP_LOGV(TAG, "Updating sensor");
  if (this->is_failed())
    return;
  if (this->busy_) {
    ESP_LOGW(TAG, "Overlapping calls to update, ignoring");
    return;
  }
  this->busy_ = true;
  ESP_LOGV(TAG, "Setting port to capacitance...");
  if (!this->set_seesaw_port_(BaseAddress::TOUCH, TouchAddress::CHAN_0)) {
    ESP_LOGW(TAG, "Failed to set seesaw port to capacitance");
    this->busy_ = false;
    return;
  }

  ESP_LOGV(TAG, "Set seesaw port");
  set_timeout("capacitance", this->read_delay_, [this]() {
    if (this->capacitance_raw_ != nullptr) {
      ESP_LOGV(TAG, "reading capacitance...");
      uint16_t raw;
      if (this->read_capacitance_(raw)) {
        float raw_f = static_cast<float>(raw);
        this->capacitance_raw_->publish_state(raw_f);
      }
    }

    ESP_LOGV(TAG, "Setting port to temperature...");
    if (!this->set_seesaw_port_(BaseAddress::STATUS, StatusAddress::TEMPERATURE)) {
      ESP_LOGW(TAG, "Failed to set seesaw port to temperature");
      this->busy_ = false;
      return;
    }
    set_timeout("temperature", this->read_delay_, [this]() {
      if (this->temperature_ != nullptr) {
        ESP_LOGV(TAG, "reading temperature...");
        float c;
        if (this->read_temp_c_(c)) {
          this->temperature_->publish_state(c);
        } else {
          ESP_LOGW(TAG, "Failed to read temperature");
        }
      }
      this->busy_ = false;
      return;
    });
  });
}

bool AdafruitSoilSensor::read_temp_c_(float &temp_c) {
  uint8_t buf[4];
  if (!this->read(buf, 4)) {
    return false;
  }
  int32_t ret = ((uint32_t) buf[0] << 24) | ((uint32_t) buf[1] << 16) | ((uint32_t) buf[2] << 8) | (uint32_t) buf[3];
  temp_c = (1.0 / (1UL << 16)) * ret;
  return true;
}

bool AdafruitSoilSensor::read_capacitance_(uint16_t &touch_value) {
  uint8_t buf[2];
  if (this->read(buf, 2)) {
    touch_value = ((uint16_t) buf[0] << 8) | buf[1];
    return true;
  }
  return false;
}

bool AdafruitSoilSensor::set_seesaw_port_(uint8_t base_address, uint8_t specific_address) {
  uint8_t cmd[2] = {base_address, specific_address};
  i2c::ErrorCode res = this->write(cmd, 2);
  if (res != i2c::ErrorCode::ERROR_OK) {
    ESP_LOGW(TAG, "Error starting i2c txn");
    return false;
  }
  return true;
}

}  // namespace adafruit_soil_sensor
}  // namespace esphome
