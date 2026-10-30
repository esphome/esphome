#include "tee501.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tee501 {

ESPHOME_LOG_TAG(TAG, "tee501");

static constexpr size_t TEE501_SERIAL_NUMBER_SIZE = 7;

void TEE501Component::setup() {
  uint8_t address[] = {0x70, 0x29};
  uint8_t identification[9];
  if (this->write_read(address, sizeof address, identification, sizeof identification) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (identification[8] != crc8(identification, 8, 0xFF, 0x31, true)) {
    this->mark_failed(LOG_STR("CRC check failed"));
    return;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char serial_hex[format_hex_size(TEE501_SERIAL_NUMBER_SIZE)];
#endif
  ESP_LOGV(TAG, "    Serial Number: 0x%s", format_hex_to(serial_hex, identification, TEE501_SERIAL_NUMBER_SIZE));
}

void TEE501Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TEE501:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "TEE501", this);
}

void TEE501Component::update() {
  uint8_t address_1[] = {0x2C, 0x1B};
  if (this->write(address_1, 2) != i2c::ERROR_OK) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->set_timeout(50, [this]() {
    uint8_t i2c_response[3];
    if (this->read(i2c_response, 3) != i2c::ERROR_OK) {
      this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    if (i2c_response[2] != crc8(i2c_response, 2, 0xFF, 0x31, true)) {
      this->status_set_warning(LOG_STR("CRC check failed"));
      return;
    }
    float temperature = (float) encode_uint16(i2c_response[0], i2c_response[1]);
    if (temperature > 55536) {
      temperature = (temperature - 65536) / 100;
    } else {
      temperature = temperature / 100;
    }
    ESP_LOGD(TAG, "Got temperature=%.2f°C", temperature);
    this->publish_state(temperature);
    this->status_clear_warning();
  });
}

}  // namespace esphome::tee501
