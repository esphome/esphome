#include "hte501.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hte501 {

static const char *const TAG = "hte501";

void HTE501Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HTE501...");
  uint8_t address[] = {0x70, 0x29};
  this->write(address, 2, false);
  uint8_t identification[9];
  this->read(identification, 9);
  if (identification[8] != calc_crc8_(identification, 0, 7)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->mark_failed();
    return;
  }
  uint32_t serial_number1 =
      (identification[0] << 24) + (identification[1] << 16) + (identification[2] << 8) + identification[3];
  uint32_t serial_number2 =
      (identification[4] << 24) + (identification[5] << 16) + (identification[6] << 8) + identification[7];
  ESP_LOGV(TAG, "    Serial Number: 0x%08X%08X", serial_number1, serial_number2);
}

void HTE501Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HTE501:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with HTE501 failed!");
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
  this->write(address_1, 2, true);
  this->set_timeout(50, [this]() {
    uint8_t i2c_response[6];
    this->read(i2c_response, 6);
    if (i2c_response[2] != calcCrc8(i2c_response, 0, 1) && i2c_response[5] != calcCrc8(i2c_response, 3, 4)) {
      this->error_code_ = CRC_CHECK_FAILED;
      this->status_set_warning();
      return;
    }
    float temperature = ((float) (i2c_response[0]) * 256 + i2c_response[1]) / 100;
    float humidity = ((float) (i2c_response[3]) * 256 + i2c_response[4]) / 100;

    ESP_LOGD(TAG, "Got temperature=%.2f°C humidity=%.2f%%", temperature, humidity);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    this->status_clear_warning();
  });
}

unsigned char HTE501Component::calc_crc8_(unsigned char buf[], unsigned char from, unsigned char to) {
  unsigned char crc_val = 0xFF;
  unsigned char i = 0;
  unsigned char j = 0;
  for (i = from; i <= to; i++) {
    int cur_val = buf[i];
    for (j = 0; j < 8; j++) {
      if (((crc_val ^ cur_val) & 0x80) != 0)  //If MSBs are not equal
      {
        crc_val = ((crc_val << 1) ^ 0x31);
      } else {
        crc_val = (crc_val << 1);
      }
      cur_val = cur_val << 1;
    }
  }
  return crc_val;
}
}  // namespace hte501
}  // namespace esphome
