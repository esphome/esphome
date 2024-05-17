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
  setMeasurementMode(TEMP_AND_HUMID);

  // Set rate to manual
  uint8_t configContents = readReg(CONFIG);
  configContents &= 0x8F;
  writeReg(CONFIG, configContents);

  // Set temperature resolution to 14bit
  configContents = readReg(CONFIG);
  configContents &= 0x3F;
  writeReg(CONFIG, configContents);

  // Set humidity resolution to 14bit
  configContents = readReg(CONFIG);
  configContents &= 0xCF;
  writeReg(CONFIG, configContents);

  delay(1000);  // wait for 1 second
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
  uint8_t raw_data[4];

  // Trigger measurement
  uint8_t configContents = readReg(CONFIG);
  configContents |= 0x01;
  writeReg(CONFIG, configContents);

  delayMicroseconds(1000);  // 1ms delay after triggering the sample

  if (this->read(raw_data, 4) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  uint16_t raw_temp = (raw_data[1] << 8) | raw_data[0];
  float temp = raw_temp * 0.0025177f - 40.0f;  // raw * 2^-16 * 165 - 40
  this->temperature_->publish_state(temp);

  uint16_t raw_humidity = (raw_data[3] << 8) | raw_data[2];
  float humidity = raw_humidity * 0.0015258f;  // raw * 2^-16 * 100
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

float HDC2010Component::get_setup_priority() const { return setup_priority::DATA; }

void HDC2010Component::setMeasurementMode(int mode) {
  uint8_t configContents;
  configContents = readReg(MEASUREMENT_CONFIG);

  switch (mode) {
    case TEMP_AND_HUMID:
      configContents = (configContents & 0xF9);
      break;

    case TEMP_ONLY:
      configContents = (configContents & 0xFC);
      configContents = (configContents | 0x02);
      break;

    case HUMID_ONLY:
      configContents = (configContents & 0xFD);
      configContents = (configContents | 0x04);
      break;

    default:
      configContents = (configContents & 0xF9);
  }

  writeReg(MEASUREMENT_CONFIG, configContents);
}

}  // namespace hdc2010
}  // namespace esphome