#include "hdc2010.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
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
  configContents = read_register(MEASUREMENT_CONFIG);
  configContents = (configContents & 0xF9);  // Always set to TEMP_AND_HUMID mode
  this->write_bytes(MEASUREMENT_CONFIG, &configContents, 1);

  // Set rate to manual
  configContents = read_register(CONFIG);
  configContents &= 0x8F;
  this->write_bytes(CONFIG, &configContents, 1);

  // Set temperature resolution to 14bit
  configContents = read_register(CONFIG);
  configContents &= 0x3F;
  this->write_bytes(CONFIG, &configContents, 1);

  // Set humidity resolution to 14bit
  configContents = read_register(CONFIG);
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
  float temp = readTemp();
  float humidity = readHumidity();

  this->temperature_->publish_state(temp);
  this->humidity_->publish_state(humidity);

  ESP_LOGD(TAG, "Got temperature=%.1f°C humidity=%.1f%%", temp, humidity);
  this->status_clear_warning();
}

// void HDC2010::enableHeater()
// {
//   uint16_t configContents; //Stores current contents of config register

//   configContents = readReg(CONFIG);

//   //set bit 3 to 1 to enable heater
//   configContents = (configContents | 0x08);

// writeReg(CONFIG, configContents);

// }

// void HDC2010::disableHeater()
// {
//   uint16_t configContents; //Stores current contents of config register

//   configContents = readReg(CONFIG);

////set bit 3 to 0 to disable heater (all other bits 1)
// configContents = (configContents & 0xF7);
// writeReg(CONFIG, configContents);
// }
float HDC2010Component::readTemp() {
  uint8_t byte[2];
  uint16_t temp;

  byte[0] = read_register(HDC2010_CMD_TEMPERATURE_LOW);
  byte[1] = read_register(HDC2010_CMD_TEMPERATURE_HIGH);

  temp = (unsigned int) byte[1] << 8 | byte[0];
  return (float) temp * 0.0025177f - 40.0f;
}

float HDC2010Component::readHumidity() {
  uint8_t byte[2];
  uint16_t humidity;

  byte[0] = read_register(HDC2010_CMD_HUMIDITY_LOW);
  byte[1] = read_register(HDC2010_CMD_HUMIDITY_HIGH);

  humidity = (unsigned int) byte[1] << 8 | byte[0];
  return (float) humidity * 0.001525879f;
}
float HDC2010Component::get_setup_priority() const { return setup_priority::DATA; }
}  // namespace hdc2010
}  // namespace esphome