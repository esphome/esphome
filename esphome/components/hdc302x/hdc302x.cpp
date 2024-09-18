#include "hdc302x.h"
#include <cmath>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hdc302x {

static const char *const TAG = "hdc302x";

static const uint8_t HDC302X_CMD_TEMP_AND_HUMIDITY[2] = {0x24, 0x00};
static const uint8_t HDC302X_CMD_SOFT_RESET[2] = {0x30, 0xA2};

uint8_t crc8(const uint8_t *data, int len) {
  // Check CRC
  uint8_t crc = 0xff;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

template<size_t N> i2c::ErrorCode HDC302xComponent::safe_write(const uint8_t (&data)[N]) {
  return this->write(data, N);
}

template<size_t N> inline i2c::ErrorCode HDC302xComponent::safe_read(uint8_t (&data)[N]) { return this->read(data, N); }

void HDC302xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HDC302X...");

  if (this->safe_write(HDC302X_CMD_SOFT_RESET) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "HDC302X soft reset failed!");
    this->status_set_warning();
    return;
  }
}

void HDC302xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC302X:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with HDC302X failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}
void HDC302xComponent::update() {
  if (safe_write(HDC302X_CMD_TEMP_AND_HUMIDITY) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  this->set_timeout("read_data", 20, [this]() {
    uint8_t raw_temp_humidity[6] = {0};
    size_t raw_temp_humidity_len = sizeof(raw_temp_humidity);
    if (this->safe_read(raw_temp_humidity) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    ESP_LOGD(TAG, "Got data: %02X %02X %02X %02X %02X %02X", raw_temp_humidity[0], raw_temp_humidity[1],
             raw_temp_humidity[2], raw_temp_humidity[3], raw_temp_humidity[4], raw_temp_humidity[5]);

    const int value_len = 2;
    const int value_plus_crc_len = 3;
    uint8_t crc0 = crc8(raw_temp_humidity, value_len);
    uint8_t crc1 = crc8(raw_temp_humidity + value_plus_crc_len, value_len);

    ESP_LOGD(TAG, "CRC0: %02X, CRC1: %02X", crc0, crc1);

    bool success = true;
    if (crc0 == raw_temp_humidity[2]) {
      uint16_t raw_temp = (raw_temp_humidity[0] << 8) | raw_temp_humidity[1];
      float temp = -45.0f + 175.0f * (raw_temp / 65536.0f);
      this->temperature_->publish_state(temp);
      ESP_LOGD(TAG, "Got temperature=%.1f°C", temp);
    } else {
      ESP_LOGW(TAG, "CRC mismatch in HDC302X temp data!");
      this->status_set_warning();
      success = false;
    }

    if (crc1 == raw_temp_humidity[5]) {
      uint16_t raw_humidity = (raw_temp_humidity[3] << 8) | raw_temp_humidity[4];
      float humidity = 100.0f * (raw_humidity / 65536.0f);
      this->humidity_->publish_state(humidity);
      ESP_LOGD(TAG, "Got humidity=%.1f%%", humidity);
    } else {
      ESP_LOGW(TAG, "CRC mismatch in HDC302X humidity data!");
      this->status_set_warning();
      success = false;
    }

    if (success) {
      this->status_clear_warning();
    }
  });
}
float HDC302xComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace hdc302x
}  // namespace esphome
