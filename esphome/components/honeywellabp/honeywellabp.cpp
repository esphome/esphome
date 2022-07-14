#include "honeywellabp.h"
#include "esphome/core/log.h"

namespace esphome {
namespace honeywellabp {

static const char *const TAG = "honeywellabp";

const float MIN_COUNT = 1638.4;   // 1638 counts (10% of 2^14 counts or 0x0666)
const float MAX_COUNT = 14745.6;  // 14745 counts (90% of 2^14 counts or 0x3999)

void HONEYWELLABPSensor::setup() {
  ESP_LOGD(TAG, "Setting up Honeywell ABP Sensor ");
  this->spi_setup();
}

uint8_t HONEYWELLABPSensor::readsensor_() {
  // Polls the sensor for new data.
  // transfer 4 bytes (the last two are temperature only used by some sensors)
  this->enable();
  buf_[0] = this->read_byte();
  buf_[1] = this->read_byte();
  buf_[2] = this->read_byte();
  buf_[3] = this->read_byte();
  this->disable();

  // Check the status codes:
  // status = 0 : normal operation
  // status = 1 : device in command mode
  // status = 2 : stale data
  // status = 3 : diagnostic condition
  status_ = buf_[0] >> 6 & 0x3;
  ESP_LOGV(TAG, "Sensor status %d", status_);

  // if device is normal and there is new data, bitmask and save the raw data
  if (status_ == 0) {
    // 14 - bit pressure is the last 6 bits of byte 0 (high bits) & all of byte 1 (lowest 8 bits)
    pressure_count_ = ((uint16_t)(buf_[0]) << 8 & 0x3F00) | ((uint16_t)(buf_[1]) & 0xFF);
    // 11 - bit temperature is all of byte 2 (lowest 8 bits) and the first three bits of byte 3
    temperature_count_ = (((uint16_t)(buf_[2]) << 3) & 0x7F8) | (((uint16_t)(buf_[3]) >> 5) & 0x7);
    ESP_LOGV(TAG, "Sensor pressure_count_ %d", pressure_count_);
    ESP_LOGV(TAG, "Sensor temperature_count_ %d", temperature_count_);
  }
  return status_;
}

// returns status
uint8_t HONEYWELLABPSensor::readstatus_() { return status_; }

// The pressure value from the most recent reading in raw counts
int HONEYWELLABPSensor::rawpressure_() { return pressure_count_; }

// The temperature value from the most recent reading in raw counts
int HONEYWELLABPSensor::rawtemperature_() { return temperature_count_; }

// Converts a digital pressure measurement in counts to pressure measured
float HONEYWELLABPSensor::countstopressure_(const int counts, const float min_pressure, const float max_pressure) {
  return ((((float) counts - MIN_COUNT) * (max_pressure - min_pressure)) / (MAX_COUNT - MIN_COUNT)) + min_pressure;
}

// Converts a digital temperature measurement in counts to temperature in C
// This will be invalid if sensore daoes not have temperature measurement capability
float HONEYWELLABPSensor::countstotemperatures_(const int counts) { return (((float) counts / 2047.0) * 200.0) - 50.0; }

// Pressure value from the most recent reading in units
float HONEYWELLABPSensor::read_pressure_() {
  return countstopressure_(pressure_count_, honeywellabp_min_pressure_, honeywellabp_max_pressure_);
}

// Temperature value from the most recent reading in degrees C
float HONEYWELLABPSensor::read_temperature_() { return countstotemperatures_(temperature_count_); }

void HONEYWELLABPSensor::update() {
  ESP_LOGV(TAG, "Update Honeywell ABP Sensor");
  if (readsensor_() == 0) {
    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(read_pressure_() * 1.0);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(read_temperature_() * 1.0);
  }
}

float HONEYWELLABPSensor::get_setup_priority() const { return setup_priority::LATE; }

void HONEYWELLABPSensor::dump_config() {
  //  LOG_SENSOR("", "HONEYWELLABP", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Min Pressure Range: %0.1f", honeywellabp_min_pressure_);
  ESP_LOGCONFIG(TAG, "  Max Pressure Range: %0.1f", honeywellabp_max_pressure_);
  LOG_UPDATE_INTERVAL(this);
}

void HONEYWELLABPSensor::set_honeywellabp_min_pressure(float min_pressure) {
  this->honeywellabp_min_pressure_ = min_pressure;
}

void HONEYWELLABPSensor::set_honeywellabp_max_pressure(float max_pressure) {
  this->honeywellabp_max_pressure_ = max_pressure;
}

}  // namespace honeywellabp
}  // namespace esphome
