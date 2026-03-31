#include "hdc2080.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hdc2080 {

static const char *const TAG = "hdc2080";

static const uint8_t HDC2080_CMD_CONFIGURATION = 0x0E;
static const uint8_t HDC2080_CMD_MEASUREMENT_CONFIGURATION = 0x0F;
static const uint8_t HDC2080_CMD_TEMPERATURE = 0x00;

void HDC2080Component::setup() {
  const uint8_t data = 0b00000000;  // automatic measurement mode disabled, heater off

  if (this->write_register(HDC2080_CMD_CONFIGURATION, &data, 1) != i2c::ERROR_OK) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

void HDC2080Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC2080:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void HDC2080Component::update() {
  uint8_t data = 0b00000001;  // resolution 14bit, sample both humidity and temperature, start measurement

  if ((this->temperature_sensor_ != nullptr) && (this->humidity_sensor_ == nullptr)) {
    data = 0b00000011;  // measure temperature only
  } else if ((this->temperature_sensor_ == nullptr) && (this->humidity_sensor_ != nullptr)) {
    data = 0b00000101;  // measure humidity only
  }
  // start the conversion
  if (this->write_register(HDC2080_CMD_MEASUREMENT_CONFIGURATION, &data, 1) != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  // wait for conversion to complete 2ms should be enough, more is fine
  this->set_timeout(5, [this]() {
    uint8_t raw_data[4];
    if (this->read_register(HDC2080_CMD_TEMPERATURE, raw_data, 4) != i2c::ERROR_OK) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    this->status_clear_warning();
    // grab temperature if needed
    if (this->temperature_sensor_ != nullptr) {
      // temperature is (raw / 2^16) * 165 - 40.5
      float temp = encode_uint16(raw_data[1], raw_data[0]) * 0.0025177f - 40.5f;
      this->temperature_sensor_->publish_state(temp);
    }
    // grab humidity if needed
    if (this->humidity_sensor_ != nullptr) {
      // humidity is (raw / 2^16) * 100
      float humidity = encode_uint16(raw_data[3], raw_data[2]) * 0.001525879f;
      this->humidity_sensor_->publish_state(humidity);
    }
  });
}

}  // namespace esphome::hdc2080
