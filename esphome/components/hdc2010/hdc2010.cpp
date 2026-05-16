#include "esphome/core/hal.h"
#include "hdc2010.h"
// https://github.com/vigsterkr/homebridge-hdc2010/blob/main/src/hdc2010.js
// https://github.com/lime-labs/HDC2080-Arduino/blob/master/src/HDC2080.cpp

namespace esphome::hdc2010 {

static const char *const TAG = "hdc2010";

// Register addresses
static constexpr uint8_t REG_TEMPERATURE_LOW = 0x00;
static constexpr uint8_t REG_TEMPERATURE_HIGH = 0x01;
static constexpr uint8_t REG_HUMIDITY_LOW = 0x02;
static constexpr uint8_t REG_HUMIDITY_HIGH = 0x03;
static constexpr uint8_t REG_RESET_DRDY_INT_CONF = 0x0E;
static constexpr uint8_t REG_MEASUREMENT_CONF = 0x0F;

// REG_MEASUREMENT_CONF (0x0F) bit masks
static constexpr uint8_t MEAS_TRIG = 0x01;       // Bit 0: measurement trigger
static constexpr uint8_t MEAS_CONF_MASK = 0x06;  // Bits 2:1: measurement mode
static constexpr uint8_t HRES_MASK = 0x30;       // Bits 5:4: humidity resolution
static constexpr uint8_t TRES_MASK = 0xC0;       // Bits 7:6: temperature resolution

// REG_RESET_DRDY_INT_CONF (0x0E) bit masks
static constexpr uint8_t AMM_MASK = 0x70;  // Bits 6:4: auto measurement mode

void HDC2010Component::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  // Set 14-bit resolution for both sensors and measurement mode to temp + humidity
  uint8_t config_contents;
  this->read_register(REG_MEASUREMENT_CONF, &config_contents, 1);
  config_contents &= ~(TRES_MASK | HRES_MASK | MEAS_CONF_MASK);  // 14-bit temp, 14-bit humidity, temp+humidity mode
  this->write_bytes(REG_MEASUREMENT_CONF, &config_contents, 1);

  // Set auto measurement rate to manual (on-demand via MEAS_TRIG)
  this->read_register(REG_RESET_DRDY_INT_CONF, &config_contents, 1);
  config_contents &= ~AMM_MASK;
  this->write_bytes(REG_RESET_DRDY_INT_CONF, &config_contents, 1);
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
  this->read_register(REG_MEASUREMENT_CONF, &config_contents, 1);
  config_contents |= MEAS_TRIG;
  this->write_bytes(REG_MEASUREMENT_CONF, &config_contents, 1);

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

  this->read_register(REG_TEMPERATURE_LOW, &byte[0], 1);
  this->read_register(REG_TEMPERATURE_HIGH, &byte[1], 1);

  uint16_t temp = encode_uint16(byte[1], byte[0]);
  return (float) temp * 0.0025177f - 40.0f;
}

float HDC2010Component::read_humidity() {
  uint8_t byte[2];

  this->read_register(REG_HUMIDITY_LOW, &byte[0], 1);
  this->read_register(REG_HUMIDITY_HIGH, &byte[1], 1);

  uint16_t humidity = encode_uint16(byte[1], byte[0]);
  return (float) humidity * 0.001525879f;
}

}  // namespace esphome::hdc2010
