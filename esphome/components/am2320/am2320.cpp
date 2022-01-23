// Implementation based on:
//  - ESPEasy: https://github.com/letscontrolit/ESPEasy/blob/mega/src/_P034_DHT12.ino
//  - DHT12_sensor_library: https://github.com/xreef/DHT12_sensor_library/blob/master/DHT12.cpp
//  - Arduino - AM2320: https://github.com/EngDial/AM2320/blob/master/src/AM2320.cpp

#include "am2320.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace am2320 {

static const char *const TAG = "am2320";

// ---=== Calc CRC16 ===---
uint16_t crc_16(uint8_t *ptr, uint8_t length) {
  uint16_t crc = 0xFFFF;
  uint8_t i;
  //------------------------------
  while (length--) {
    crc ^= *ptr++;
    for (i = 0; i < 8; i++)
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else
        crc >>= 1;
  }
  return crc;
}

void AM2320Component::update() {
  uint8_t data[8];
  data[0] = 0;
  data[1] = 4;
  if (!this->read_data_(data)) {
    this->status_set_warning();
    return;
  }

  float temperature = (((data[4] & 0x7F) << 8) + data[5]) / 10.0f;
  temperature = (data[4] & 0x80) ? -temperature : temperature;
  float humidity = ((data[2] << 8) + data[3]) / 10.0f;

  ESP_LOGD(TAG, "Got temperature=%.1f°C humidity=%.1f%%", temperature, humidity);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);
  this->status_clear_warning();
}
void AM2320Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AM2320...");
  uint8_t data[8];
  data[0] = 0;
  data[1] = 4;
  if (!this->read_data_(data)) {
    this->mark_failed();
    return;
  }
}
void AM2320Component::dump_config() {
  ESP_LOGD(TAG, "AM2320:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AM2320 failed!");
  }
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}
float AM2320Component::get_setup_priority() const { return setup_priority::DATA; }

bool AM2320Component::read_bytes_(uint8_t a_register, uint8_t *data, uint8_t len, uint32_t conversion) {
  if (!this->write_bytes(a_register, data, 2)) {
    ESP_LOGW(TAG, "Writing bytes for AM2320 failed!");
    return false;
  }

  if (conversion > 0)
    delay(conversion);
  return this->read(data, len) == i2c::ERROR_OK;
}

bool AM2320Component::read_data_(uint8_t *data) {
  // Wake up
  this->write_bytes(0, data, 0);

  // Write instruction 3, 2 bytes, get 8 bytes back (2 preamble, 2 bytes temperature, 2 bytes humidity, 2 bytes CRC)
  if (!this->read_bytes_(3, data, 8, 2)) {
    ESP_LOGW(TAG, "Updating AM2320 failed!");
    return false;
  }

  uint16_t checksum;

  checksum = data[7] << 8;
  checksum += data[6];

  if (crc_16(data, 6) != checksum) {
    ESP_LOGW(TAG, "AM2320 Checksum invalid!");
    return false;
  }

  return true;
}

}  // namespace am2320
}  // namespace esphome
