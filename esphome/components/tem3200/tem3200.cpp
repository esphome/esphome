#include "tem3200.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tem3200 {

static const char *const TAG = "tem3200";

enum ErrorCode {
  NONE = 0,
  RESERVED = 1,
  STALE = 2,
  FAULT = 3,
};

void TEM3200Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TEM3200...");

  uint8_t status(NONE);
  uint16_t raw_temperature(0);
  uint16_t raw_pressure(0);

  i2c::ErrorCode err = this->read_(status, raw_temperature, raw_pressure);
  if (err != i2c::ERROR_OK) {
    ESP_LOGCONFIG(TAG, "    I2C Communication Failed...");
    this->mark_failed();
    return;
  }

  switch (status) {
    case RESERVED:
      ESP_LOGE(TAG, "Invalid RESERVED Device Status");
      this->mark_failed();
      return;
    case FAULT:
      ESP_LOGE(TAG, "FAULT condition in the SSC or sensing element");
      this->mark_failed();
      return;
    case STALE:
      ESP_LOGE(TAG, "STALE data. Data has not been updated since last fetch");
      this->status_set_warning();
      break;
  }
  ESP_LOGCONFIG(TAG, "    Success...");
}

void TEM3200Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TEM3200:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Raw Pressure", this->raw_pressure_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

float TEM3200Component::get_setup_priority() const { return setup_priority::DATA; }

i2c::ErrorCode TEM3200Component::read_(uint8_t &status, uint16_t &raw_temperature, uint16_t &raw_pressure) {
  uint8_t response[4] = {0x00, 0x00, 0x00, 0x00};

  // initiate data read
  i2c::ErrorCode err = this->read(response, 4);
  if (err != i2c::ERROR_OK) {
    return err;
  }

  // extract top 2 bits of first byte for status
  status = (ErrorCode) (response[0] & 0xc0) >> 6;
  if (status == RESERVED || status == FAULT) {
    return i2c::ERROR_OK;
  }

  // if data is stale; reread
  if (status == STALE) {
    // wait for measurement 2ms
    delay(2);

    err = this->read(response, 4);
    if (err != i2c::ERROR_OK) {
      return err;
    }
  }

  // extract top 2 bits of first byte for status
  status = (ErrorCode) (response[0] & 0xc0) >> 6;
  if (status == RESERVED || status == FAULT) {
    return i2c::ERROR_OK;
  }

  // extract top 6 bits of first byte and all bits of second byte for pressure
  raw_pressure = (((response[0] & 0x3f)) << 8 | response[1]);

  // extract all bytes of 3rd byte and top 3 bits of fourth byte for temperature
  raw_temperature = ((response[2] << 3) | (response[3] & 0xe0) >> 5);

  return i2c::ERROR_OK;
}

inline float convert_temperature(uint16_t raw_temperature) {
  const float temperature_bits_span = 2048;
  const float temperature_max = 150;
  const float temperature_min = -50;
  const float temperature_span = temperature_max - temperature_min;

  float temperature = (raw_temperature * temperature_span / temperature_bits_span) + temperature_min;

  return temperature;
}

void TEM3200Component::update() {
  uint8_t status(NONE);
  uint16_t raw_temperature(0);
  uint16_t raw_pressure(0);
  i2c::ErrorCode err = this->read_(status, raw_temperature, raw_pressure);

  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C Communication Failed");
    this->status_set_warning();
    return;
  }

  switch (status) {
    case RESERVED:
      ESP_LOGE(TAG, "Failed: Device return RESERVED status");
      this->status_set_warning();
      return;
    case FAULT:
      ESP_LOGE(TAG, "Failed: FAULT condition in the SSC or sensing element");
      this->mark_failed();
      return;
    case STALE:
      ESP_LOGE(TAG, "Warning: STALE data. Data has not been updated since last fetch");
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

}  // namespace tem3200
}  // namespace esphome
