#include "esphome/core/log.h"
#include "sgp40.h"

namespace esphome {
namespace sgp40 {

static const char *TAG = "sgp40";

void SGP40Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SGP40...");

  // Serial Number identification
  if (!this->write_command_(SGP40_CMD_GET_SERIAL_ID)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  uint16_t raw_serial_number[3];

  if (!this->read_data_(raw_serial_number, 3)) {
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint64_t(raw_serial_number[0]) << 24) | (uint64_t(raw_serial_number[1]) << 16) |
                         (uint64_t(raw_serial_number[2]));
  ESP_LOGD(TAG, "Serial Number: %llu", this->serial_number_);

  // Featureset identification for future use
  if (!this->write_command_(SGP40_CMD_GET_FEATURESET)) {
    ESP_LOGD(TAG, "raw_featureset write_command_ failed");
    this->mark_failed();
    return;
  }
  uint16_t raw_featureset[1];
  if (!this->read_data_(raw_featureset, 1)) {
    ESP_LOGD(TAG, "raw_featureset read_data_ failed");
    this->mark_failed();
    return;
  }

  this->featureset_ = raw_featureset[0];
  if ((this->featureset_ & 0x1FF) != SGP40_FEATURESET) {
    ESP_LOGD(TAG, "Product feature set failed 0x%0X , expecting 0x%0X", uint16_t(this->featureset_ & 0x1FF),
             SGP40_FEATURESET);
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Product version: 0x%0X", uint16_t(this->featureset_ & 0x1FF));

  VocAlgorithm_init(&this->voc_algorithm_params_);

  this->self_test_();
}

bool SGP40Component::self_test_() {
  uint16_t reply[1];

  ESP_LOGD(TAG, "selfTest start");
  if (!this->write_command_(SGP40_CMD_SELF_TEST)) {
    this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGD(TAG, "selfTest SGP40_CMD_SELF_TEST failed");
    this->mark_failed();
    return false;
  }
  delay(250);  // NOLINT
  if (!this->read_data_(reply, 1)) {
    ESP_LOGD(TAG, "selfTest read_data_ failed");
    this->mark_failed();
    return false;
  }

  if (reply[0] == 0xD400) {
    ESP_LOGD(TAG, "selfTest completed");
    return true;
  }
  ESP_LOGD(TAG, "selfTest failed");
  this->mark_failed();
  return false;
}

void SGP40Component::update() {}
void SGP40Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SGP40:");
  LOG_I2C_DEVICE(this);
}

bool SGP40Component::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

uint8_t SGP40Component::sht_crc_(uint8_t data1, uint8_t data2) {
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

bool SGP40Component::read_data_(uint16_t *data, uint8_t len) {
  const uint8_t num_bytes = len * 3;
  auto *buf = new uint8_t[num_bytes];

  if (!this->parent_->raw_receive(this->address_, buf, num_bytes)) {
    delete[](buf);
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    const uint8_t j = 3 * i;
    uint8_t crc = sht_crc_(buf[j], buf[j + 1]);
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

}  // namespace sgp40
}  // namespace esphome
