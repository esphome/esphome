#include "sts3x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sts3x {

static const char *TAG = "sts3x";

static const uint16_t STS3X_COMMAND_READ_SERIAL_NUMBER = 0x3780;
static const uint16_t STS3X_COMMAND_READ_STATUS = 0xF32D;
static const uint16_t STS3X_COMMAND_SOFT_RESET = 0x30A2;
static const uint16_t STS3X_COMMAND_POLLING_H = 0x2400;

/// Commands for future use
static const uint16_t STS3X_COMMAND_CLEAR_STATUS = 0x3041;
static const uint16_t STS3X_COMMAND_HEATER_ENABLE = 0x306D;
static const uint16_t STS3X_COMMAND_HEATER_DISABLE = 0x3066;
static const uint16_t STS3X_COMMAND_FETCH_DATA = 0xE000;

void STS3XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STS3x...");
  if (!this->write_command_(STS3X_COMMAND_READ_SERIAL_NUMBER)) {
    this->mark_failed();
    return;
  }

  uint16_t raw_serial_number[2];
  if (!this->read_data_(raw_serial_number, 1)) {
    this->mark_failed();
    return;
  }
  uint32_t serial_number = (uint32_t(raw_serial_number[0]) << 16);
  ESP_LOGV(TAG, "    Serial Number: 0x%08X", serial_number);
}
void STS3XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "STS3x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ST3x failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "STS3x", this);
}
float STS3XComponent::get_setup_priority() const { return setup_priority::DATA; }
void STS3XComponent::update() {
  if (this->status_has_warning()) {
    ESP_LOGD(TAG, "Retrying to reconnect the sensor.");
    this->write_command_(STS3X_COMMAND_SOFT_RESET);
  }
  if (!this->write_command_(STS3X_COMMAND_POLLING_H)) {
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[1];
    if (!this->read_data_(raw_data, 1)) {
      this->status_set_warning();
      return;
    }

    float temperature = 175.0f * float(raw_data[0]) / 65535.0f - 45.0f;
    ESP_LOGD(TAG, "Got temperature=%.2f°C", temperature);
    this->publish_state(temperature);
    this->status_clear_warning();
  });
}

bool STS3XComponent::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

uint8_t sts3x_crc(uint8_t data1, uint8_t data2) {
  uint8_t bit;
  uint8_t crc = 0xFF;

  crc ^= data1;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80)
      crc = (crc << 1) ^ 0x131;
    else
      crc = (crc << 1);
  }

  crc ^= data2;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80)
      crc = (crc << 1) ^ 0x131;
    else
      crc = (crc << 1);
  }

  return crc;
}

bool STS3XComponent::read_data_(uint16_t *data, uint8_t len) {
  const uint8_t num_bytes = len * 3;
  auto *buf = new uint8_t[num_bytes];

  if (!this->parent_->raw_receive(this->address_, buf, num_bytes)) {
    delete[](buf);
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    const uint8_t j = 3 * i;
    uint8_t crc = sts3x_crc(buf[j], buf[j + 1]);
    if (crc != buf[j + 2]) {
      ESP_LOGE(TAG, "CRC8 Checksum invalid! 0x%02X != 0x%02X", buf[j + 2], crc);
      delete[](buf);
      return false;
    }
    data[i] = (buf[j] << 8) | buf[j + 1];
  }

  delete[](buf);
  return true;
}

}  // namespace sts3x
}  // namespace esphome
