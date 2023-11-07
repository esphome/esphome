#include "sht2x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sht2x {

static const char *const TAG = "sht2x";

static const uint8_t SHT2X_COMMAND_TEMPERATURE = 0xF3;
static const uint8_t SHT2X_COMMAND_HUMIDITY = 0xF5;
static const uint8_t SHT2X_COMMAND_SOFT_RESET = 0xFE;

static const uint16_t SHT2X_DELAY_TEMPERATURE = 85;
static const uint16_t SHT2X_DELAY_HUMIDITY = 30;

// Thank you @RobTillaart for this
// https://github.com/RobTillaart/SHT2x/tree/master
uint8_t SHT2XComponent::crc8_(const uint8_t *data, uint8_t len) {
  //  CRC-8 formula from page 14 of SHT spec pdf
  //  Sensirion_Humidity_Sensors_SHT2x_CRC_Calculation.pdf
  const uint8_t poly = 0x31;
  uint8_t crc = 0x00;

  for (uint8_t j = len; j; --j) {
    crc ^= *data++;

    for (uint8_t i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ poly : (crc << 1);
    }
  }
  return crc;
}

void SHT2XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sht2x...");
  if (!this->write_command(SHT2X_COMMAND_SOFT_RESET)) {
    this->mark_failed();
    return;
  }
}

void SHT2XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sht2x:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SHT2X failed!");
  }

  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

uint16_t SHT2XComponent::read_raw_value_() {
  uint8_t buffer[3];
  uint8_t crc;
  uint16_t result;

  if (this->read(buffer, 3) != i2c::ERROR_OK) {
    this->status_set_warning();
  }
  crc = this->crc8_(buffer, 2);

  if (crc != buffer[2]) {
    ESP_LOGE(TAG, "CRC8 Checksum invalid. 0x%02X != 0x%02X", buffer[2], crc);
    this->status_set_warning();
  }

  result = buffer[0] << 8;
  result += buffer[1];
  result &= 0xFFFC;

  return result;
}

float SHT2XComponent::get_temperature_() {
  if (this->write(&SHT2X_COMMAND_TEMPERATURE, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Reading temperature error");
  };

  this->set_timeout("read_temperature", SHT2X_DELAY_TEMPERATURE, [this]() {
    uint16_t raw_temperature = this->read_raw_value_();
    float temperature = -46.85 + (175.72 / 65536.0) * raw_temperature;
    return temperature;
  });
}

float SHT2XComponent::get_humidity_() {
  if (this->write(&SHT2X_COMMAND_HUMIDITY, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Reading humidity error");
  }

  this->set_timeout("read_humidity", SHT2X_DELAY_HUMIDITY, [this]() {
    uint16_t raw_humidity = this->read_raw_value_();
    float humidity = -6.0 + (125.0 / 65536.0) * raw_humidity;
    return humidity;
  });
}

void SHT2XComponent::update() {
  if (this->status_has_warning()) {
    ESP_LOGD(TAG, "Retrying to reconnect the sensor.");
    this->write_command(SHT2X_COMMAND_SOFT_RESET);
  }

  float temperature = this->get_temperature_();
  float humidity = this->get_humidity_();

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }

  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(humidity);
  }
  this->status_clear_warning();
}

float SHT2XComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace sht2x
}  // namespace esphome
