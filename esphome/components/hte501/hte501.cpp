#include "hte501.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hte501 {

static const char *const TAG = "hte501";

void HTE501Component::setup() {
  uint8_t address[] = {0x70, 0x29};
  uint8_t identification[9];
  this->write_read(address, sizeof address, identification, sizeof identification);
  if (identification[8] != crc8(identification, 8, 0xFF, 0x31, true)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "    Serial Number: 0x%s", format_hex(identification + 0, 7).c_str());
}

void HTE501Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HTE501:");
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
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

float HTE501Component::get_setup_priority() const { return setup_priority::DATA; }
void HTE501Component::update() {
  uint8_t address_1[] = {0x2C, 0x1B};
  this->write(address_1, 2);
  this->set_timeout(50, [this]() {
    uint8_t i2c_response[6];
    this->read(i2c_response, 6);
    if (i2c_response[2] != crc8(i2c_response, 2, 0xFF, 0x31, true) &&
        i2c_response[5] != crc8(i2c_response + 3, 2, 0xFF, 0x31, true)) {
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
    float humidity = ((float) encode_uint16(i2c_response[3], i2c_response[4])) / 100.0f;

    ESP_LOGD(TAG, "Got temperature=%.2f°C humidity=%.2f%%", temperature, humidity);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    this->status_clear_warning();
  });
}
}  // namespace hte501
}  // namespace esphome
