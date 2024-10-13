#include "ams5915.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ams5915 {
static const char *const TAG = "ams5915";

/* reads pressure and temperature and returns values in counts */
bool Ams5915::read_raw_data_(uint16_t *pressure_counts, uint16_t *temperature_counts) {
  i2c::ErrorCode err = this->read(this->buffer_, sizeof(this->buffer_));
  if (err != i2c::ERROR_OK) {
    return false;
  } else {
    *pressure_counts = (((uint16_t) (this->buffer_[0] & 0x3F)) << 8) + (((uint16_t) this->buffer_[1]));
    *temperature_counts = (((uint16_t) (this->buffer_[2])) << 3) + (((uint16_t) this->buffer_[3] & 0xE0) >> 5);
    return true;
  }
}

void Ams5915::setup() {
  bool read_success = false;
  // check to see if we can talk with the sensor
  for (size_t i = 0; i < Ams5915::MAX_ATTEMPTS; i++) {
    read_success = read_raw_data_(&raw_pressure_data_, &raw_temperature_data_);
    if (read_success) {
      return;
    }
    delay(10);
  }
  // read failed
  ESP_LOGE(TAG, "Failed to read from Ams5915");
  this->mark_failed();
}

void Ams5915::update() {
  // get raw pressure and temperature data off transducer
  bool read_success = read_raw_data_(&this->raw_pressure_data_, &this->raw_temperature_data_);
  if (read_success) {
    if (this->pressure_sensor_ != nullptr) {
      // convert raw_pressure_data_ to pressure, PA as noted in datasheet
      // TODO: there maybe a helper function in helpers.h that can remap the value
      float pressure =
          (((float) (this->raw_pressure_data_ - Ams5915::DIG_OUT_P_MIN)) /
               (((float) (Ams5915::DIG_PUT_P_MAX - Ams5915::DIG_OUT_P_MIN)) / ((float) (this->p_max_ - this->p_min_))) +
           (float) this->p_min_);
      this->pressure_sensor_->publish_state(pressure * Ams5915::MBAR_TO_PA);
    }

    if (this->temperature_sensor_ != nullptr) {
      // convert raw_temperature_data_ temperature, C as noted in datasheet
      float temperature = (float) ((this->raw_temperature_data_ * 200)) / 2048.0f - 50.0f;
      this->temperature_sensor_->publish_state(temperature);
    }
  } else {
    ESP_LOGE(TAG, "Failed to read from Ams5915");
  }
}

void Ams5915::dump_config() {
  ESP_LOGCONFIG(TAG, "Ams5915:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}

}  // namespace ams5915
}  // namespace esphome
