#include "tee501.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tee501 {

static const char *const TAG = "tee501";

void TEE501Component::setup() {
  uint8_t address[] = {0x70, 0x29};
  uint8_t identification[9];
  this->read(identification, 9);
  this->write_read(address, sizeof address, identification, sizeof identification);
  if (identification[8] != crc8(identification, 8, 0xFF, 0x31, true)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "    Serial Number: 0x%s", format_hex(identification + 0, 7).c_str());
}

void TEE501Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TEE501:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      break;
    case CRC_CHECK_FAILED:
      ESP_LOGE(TAG, "The crc check failed");
      break;
    case NONE:
    default:
      break;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "TEE501", this);
}

float TEE501Component::get_setup_priority() const { return setup_priority::DATA; }
void TEE501Component::update() {
  uint8_t address_1[] = {0x2C, 0x1B};
  this->write(address_1, 2);
  this->set_timeout(50, [this]() {
    uint8_t i2c_response[3];
    this->read(i2c_response, 3);
    if (i2c_response[2] != crc8(i2c_response, 2, 0xFF, 0x31, true)) {
      this->error_code_ = CRC_CHECK_FAILED;
      this->status_set_warning();
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

}  // namespace tee501
}  // namespace esphome
