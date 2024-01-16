#include "hdc2010.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
//https://github.com/vigsterkr/homebridge-hdc2010/blob/main/src/hdc2010.js
//https://github.com/lime-labs/HDC2080-Arduino/blob/master/src/HDC2080.cpp
namespace esphome {
namespace hdc2010 {

static const char *const TAG = "hdc2010";

static const uint8_t HDC2010_ADDRESS = 0x40;  // 0b1000000 or 0b1000001 from datasheet
static const uint8_t HDC2010_CMD_CONFIGURATION_MEASUREMENT = 0x0F;
static const uint8_t HDC2010_CMD_CONFIG = 0x0E;
static const uint8_t HDC2010_CMD_TEMPERATURE_LOW = 0x00;
static const uint8_t HDC2010_CMD_TEMPERATURE_HIGH = 0x01;
static const uint8_t HDC2010_CMD_HUMIDITY_LOW = 0x02;
static const uint8_t HDC2010_CMD_HUMIDITY_HIGH = 0x03;
static const uint8_t HDC2010_CMD_HEATER_HEAT0 = 0x5A;
// static const uint8_t HDC2010_TEMP_OFFSET_ADJUST = 0x08;
// static const uint8_t HDC2010_HUM_OFFSET_ADJUST = 0x09;

void HDC2010Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HDC2010...");

  const uint8_t data[2] = {
      0b00000000,  // resolution 14bit for both humidity and temperature
      0b00000000   // reserved
  };

  if (!this->write_bytes(HDC2010_CMD_CONFIGURATION_MEASUREMENT, data, 2)) {
    // as instruction is same as powerup defaults (for now), interpret as warning if this fails
    ESP_LOGW(TAG, "HDC2010 initial config instruction error");
    this->status_set_warning();
    return;
  }
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

//Temperature
  uint16_t raw_temp;
  if (this->write(&HDC2010_CMD_TEMPERATURE_LOW, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  if (this->write(&HDC2010_CMD_TEMPERATURE_HIGH, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  delay(20);
  if (this->read(reinterpret_cast<uint8_t *>(&raw_temp), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  raw_temp = (unsigned int)(HDC2010_CMD_TEMPERATURE_HIGH) << 8 | (unsigned int)(HDC2010_CMD_TEMPERATURE_LOW);
  raw_temp = i2c::i2ctohs(raw_temp);
  
  float temp = raw_temp * 0.0025177001953125f - 40.0f;  // raw * 2^-16 * 165 - 40
  this->temperature_->publish_state(temp);



//Humidity

  uint16_t raw_humidity;
  if (this->write(&HDC2010_CMD_HUMIDITY_LOW, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  if (this->write(&HDC2010_CMD_HUMIDITY_HIGH, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  delay(20);
  if (this->read(reinterpret_cast<uint8_t *>(&raw_humidity), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  raw_humidity = (unsigned int) HDC2010_CMD_HUMIDITY_HIGH << 8 | HDC2010_CMD_HUMIDITY_LOW;
  raw_humidity = i2c::i2ctohs(raw_humidity);
  float humidity = raw_humidity * 0.00152587890625f;  // raw * 2^-16 * 100
  this->humidity_->publish_state(humidity);

  ESP_LOGD(TAG, "Got temperature=%.1f°C humidity=%.1f%%", temp, humidity);
  this->status_clear_warning();

}

// void HDC2010::enableHeater()
// {
// 	uint16_t configContents;	//Stores current contents of config register
	
// 	configContents = readReg(CONFIG);
	
// 	//set bit 3 to 1 to enable heater
// 	configContents = (configContents | 0x08);
	
// 	writeReg(CONFIG, configContents);
	
// }

// void HDC2010::disableHeater()
// {
// 	uint16_t configContents;	//Stores current contents of config register
	
// 	configContents = readReg(CONFIG);
	
// 	//set bit 3 to 0 to disable heater (all other bits 1)
// 	configContents = (configContents & 0xF7);
// 	writeReg(CONFIG, configContents);
	
// }

float HDC2010Component::get_setup_priority() const { return setup_priority::DATA;}

}  // namespace hdc2010
}  // namespace esphome