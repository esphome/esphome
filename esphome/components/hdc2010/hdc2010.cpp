#include "esphome/core/hal.h"
#include "hdc2010.h"
// https://github.com/vigsterkr/homebridge-hdc2010/blob/main/src/hdc2010.js
// https://github.com/lime-labs/HDC2080-Arduino/blob/master/src/HDC2080.cpp
namespace esphome {
namespace hdc2010 {

static const char *const TAG = "hdc2010";

static const uint8_t HDC2010_ADDRESS = 0x40;  // 0b1000000 or 0b1000001 from datasheet
static const uint8_t HDC2010_CMD_CONFIGURATION_MEASUREMENT = 0x8F;
static const uint8_t HDC2010_CMD_START_MEASUREMENT = 0xF9;
static const uint8_t HDC2010_CMD_TEMPERATURE_LOW = 0x00;
static const uint8_t HDC2010_CMD_TEMPERATURE_HIGH = 0x01;
static const uint8_t HDC2010_CMD_HUMIDITY_LOW = 0x02;
static const uint8_t HDC2010_CMD_HUMIDITY_HIGH = 0x03;
static const uint8_t CONFIG = 0x0E;
static const uint8_t MEASUREMENT_CONFIG = 0x0F;

void HDC2010Component::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  const uint8_t data[2] = {
      0b00000000,  // resolution 14bit for both humidity and temperature
      0b00000000   // reserved
  };

  if (!this->write_bytes(HDC2010_CMD_CONFIGURATION_MEASUREMENT, data, 2)) {
    ESP_LOGW(TAG, "Initial config instruction error");
    this->status_set_warning();
    return;
  }

  // Set measurement mode to temperature and humidity
  uint8_t config_contents;
  this->read_register(MEASUREMENT_CONFIG, &config_contents, 1);
  config_contents = (config_contents & 0xF9);  // Always set to TEMP_AND_HUMID mode
  this->write_bytes(MEASUREMENT_CONFIG, &config_contents, 1);

  // Set rate to manual
  this->read_register(CONFIG, &config_contents, 1);
  config_contents &= 0x8F;
  this->write_bytes(CONFIG, &config_contents, 1);

  // Set temperature resolution to 14bit
  this->read_register(CONFIG, &config_contents, 1);
  config_contents &= 0x3F;
  this->write_bytes(CONFIG, &config_contents, 1);

  // Set humidity resolution to 14bit
  this->read_register(CONFIG, &config_contents, 1);
  config_contents &= 0xCF;
  this->write_bytes(CONFIG, &config_contents, 1);
}

void HDC2010Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC2010:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void HDC2010Component::update() {
  // Trigger measurement
  uint8_t config_contents;
  this->read_register(CONFIG, &config_contents, 1);
  config_contents |= 0x01;
  this->write_bytes(MEASUREMENT_CONFIG, &config_contents, 1);

  // 1ms delay after triggering the sample
  set_timeout(1, [this]() {
    if (this->temperature_sensor_ != nullptr) {
      float temp = this->read_temp();
      this->temperature_sensor_->publish_state(temp);
      ESP_LOGD(TAG, "Temp=%.1f°C", temp);
    }

    if (this->humidity_sensor_ != nullptr) {
      float humidity = this->read_humidity();
      this->humidity_sensor_->publish_state(humidity);
      ESP_LOGD(TAG, "Humidity=%.1f%%", humidity);
    }
  });
}

float HDC2010Component::read_temp() {
  uint8_t byte[2];

  this->read_register(HDC2010_CMD_TEMPERATURE_LOW, &byte[0], 1);
  this->read_register(HDC2010_CMD_TEMPERATURE_HIGH, &byte[1], 1);

  uint16_t temp = encode_uint16(byte[1], byte[0]);
  return (float) temp * 0.0025177f - 40.0f;
}

float HDC2010Component::read_humidity() {
  uint8_t byte[2];

  this->read_register(HDC2010_CMD_HUMIDITY_LOW, &byte[0], 1);
  this->read_register(HDC2010_CMD_HUMIDITY_HIGH, &byte[1], 1);

  uint16_t humidity = encode_uint16(byte[1], byte[0]);
  return (float) humidity * 0.001525879f;
}

}  // namespace hdc2010
}  // namespace esphome
