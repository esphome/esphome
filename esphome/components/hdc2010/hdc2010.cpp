#include "esphome/core/hal.h"
#include "esphome/core/log.h"
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
  ESP_LOGCONFIG(TAG, "Setting up HDC2010...");

  const uint8_t data[2] = {
      0b00000000,  // resolution 14bit for both humidity and temperature
      0b00000000   // reserved
  };

  if (!this->write_bytes(HDC2010_CMD_CONFIGURATION_MEASUREMENT, data, 2)) {
    ESP_LOGW(TAG, "HDC2010 initial config instruction error");
    this->status_set_warning();
    return;
  }

  // Set measurement mode to temperature and humidity
  uint8_t configContents;
  read_register(MEASUREMENT_CONFIG, &configContents, 1);
  configContents = (configContents & 0xF9);  // Always set to TEMP_AND_HUMID mode
  this->write_bytes(MEASUREMENT_CONFIG, &configContents, 1);

  // Set rate to manual
  read_register(CONFIG, &configContents, 1);
  configContents &= 0x8F;
  this->write_bytes(CONFIG, &configContents, 1);

  // Set temperature resolution to 14bit
  read_register(CONFIG, &configContents, 1);
  configContents &= 0x3F;
  this->write_bytes(CONFIG, &configContents, 1);

  // Set humidity resolution to 14bit
  read_register(CONFIG, &configContents, 1);
  configContents &= 0xCF;
  this->write_bytes(CONFIG, &configContents, 1);

  delayMicroseconds(5000);  // wait for 5ms
}

void HDC2010Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC2010:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with HDC2010 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}

void HDC2010Component::update() {
  // uint8_t tempLow, tempHigh, humidLow, humidHigh;

  // Trigger measurement
  uint8_t configContents;
  configContents = readReg(MEASUREMENT_CONFIG);
  configContents = (configContents | 0x01);
  writeReg(MEASUREMENT_CONFIG, configContents);

  // delayMicroseconds(1000);  // 1ms delay after triggering the sample

  // if (!read_register(HDC2010_CMD_TEMPERATURE_LOW, &tempLow, 1) ||
  //     !read_register(HDC2010_CMD_TEMPERATURE_HIGH, &tempHigh, 1) ||
  //     !read_register(HDC2010_CMD_HUMIDITY_LOW, &humidLow, 1) ||
  //     !read_register(HDC2010_CMD_HUMIDITY_HIGH, &humidHigh, 1)) {
  //   this->status_set_warning();
  //   return;
  // }

  float temp = readTemp();
  float humidity = readHumidity();

  this->temperature_->publish_state(temp);
  this->humidity_->publish_state(humidity);

  ESP_LOGD(TAG, "Got temperature=%.1f°C humidity=%.1f%%", temp, humidity);
  this->status_clear_warning();
}

// void HDC2010::enableHeater()
// {
//   uint16_t configContents; //Stores current#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "hdc2010.h"
// https://github.com/vigsterkr/homebridge-hdc2010/blob/main/src/hdc2010.js
// https://github.com/lime-labs/HDC2080-Arduino/blob/master/src/HDC2080.cpp
namespace esphome {
namespace hdc2010 {
