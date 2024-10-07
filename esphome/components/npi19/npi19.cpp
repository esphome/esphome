#include "npi19.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace npi19 {

static const char *const TAG = "npi19";

static const uint8_t READ_COMMAND = 0xAC;

void NPI19Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NPI19...");

  uint16_t raw_temperature(0);
  uint16_t raw_pressure(0);
  i2c::ErrorCode err = this->read_(raw_temperature, raw_pressure);
  if (err != i2c::ERROR_OK) {
    ESP_LOGCONFIG(TAG, "    I2C Communication Failed...");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "    Success...");
}

void NPI19Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NPI19:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Raw Pressure", this->raw_pressure_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

float NPI19Component::get_setup_priority() const { return setup_priority::DATA; }

i2c::ErrorCode NPI19Component::read_(uint16_t &raw_temperature, uint16_t &raw_pressure) {
  // initiate data read from device
  i2c::ErrorCode w_err = write(&READ_COMMAND, sizeof(READ_COMMAND), true);
  if (w_err != i2c::ERROR_OK) {
    return w_err;
  }

  // read 4 bytes from senesor
  uint8_t response[4] = {0x00, 0x00, 0x00, 0x00};
  i2c::ErrorCode r_err = this->read(response, 4);

  if (r_err != i2c::ERROR_OK) {
    return r_err;
  }

  // extract top 6 bits of first byte and all bits of second byte for pressure
  raw_pressure = ((response[0] & 0x3F) << 8) | response[1];

  // extract all bytes of 3rd byte and top 3 bits of fourth byte for temperature
  raw_temperature = (response[2] << 3) | ((response[3] & 0xE0) >> 5);

  return i2c::ERROR_OK;
}

inline float convert_temperature(uint16_t raw_temperature) {
  /*
   * Correspondance with Amphenol confirmed the appropriate equation for computing temperature is:
   * T (°C) =(((((Th*8)+Tl)/2048)*200)-50), where Th is the high (third) byte and Tl is the low (fourth) byte.
   *
   * Tl is actually the upper 3 bits of the fourth data byte; the first 5 (LSBs) must be masked out.
   *
   *
   * The NPI-19 I2C has a temperature output, however the manufacturer does
   * not specify its accuracy on the published datasheet. They indicate
   * that the sensor should not be used as a calibrated temperature
   * reading; it’s only intended for curve fitting data during
   * compensation.
   */
  const float temperature_bits_span = 2048;
  const float temperature_max = 150;
  const float temperature_min = -50;
  const float temperature_span = temperature_max - temperature_min;

  float temperature = (raw_temperature * temperature_span / temperature_bits_span) + temperature_min;

  return temperature;
}

void NPI19Component::update() {
  uint16_t raw_temperature(0);
  uint16_t raw_pressure(0);

  i2c::ErrorCode err = this->read_(raw_temperature, raw_pressure);

  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C Communication Failed");
    this->status_set_warning();
    return;
  }

  float temperature = convert_temperature(raw_temperature);

  ESP_LOGD(TAG, "Got raw pressure=%d, temperature=%.1f°C", raw_pressure, temperature);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->raw_pressure_sensor_ != nullptr)
    this->raw_pressure_sensor_->publish_state(raw_pressure);

  this->status_clear_warning();
}

}  // namespace npi19
}  // namespace esphome
