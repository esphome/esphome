#include "edpbox.h"
#include "esphome/core/log.h"

namespace esphome {
namespace edpbox {

static const char *TAG = "edpbox";

static const uint8_t EDPBOX_INDEX = 1;

void EDPBOX::on_modbus_data(const std::vector<uint8_t> &data) {

  if (data.size() < 10) {
    ESP_LOGW(TAG, "Invalid size for EDPBOX!");
    return;
  }

  // 01 04 0e 09 21 00 37 09 32 00 01 09 2f 00 0c 00 45 65 9a 
  // Id Cc Sz Volt  Curr  Volt  Curr  Volt  Curr  Curtt crc
  //           0     2    4     6     8     10    12

  auto edpbox_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto edpbox_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(edpbox_get_16bit(i + 2)) << 16) | (uint32_t(edpbox_get_16bit(i + 0)) << 0);
  };

  // decode
  
  // auto edpbox_check = uint8_t EDPBOX_INDEX;

  //switch (edpbox_check) {
  //  case 1: {

  uint16_t raw_voltage = edpbox_get_16bit(0);
  float voltage = raw_voltage / 10.0f;  // V

  uint32_t raw_current = edpbox_get_16bit(2);
  float current = raw_current / 10.0f;  // A

  //   break;
  // }
  // case 2: {
  //   break;
  // }
  // } // end switch

  ESP_LOGD(TAG, "EDPBOX: reading...");

  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
}

void EDPBOX::update() {
  // function register_start count
  // 6C = 108
  // uint8_t EDPBOX_INDEX = 1;

  this->send(0x04, 108, 7);

  // delay(1500); not allowed :(
  // test
  // uint8_t EDPBOX_INDEX = 2;
  // this->send(0x04, 110, 7);
  // delay(1500);
}

void EDPBOX::dump_config() {
  ESP_LOGCONFIG(TAG, "EDPBOX: dump...");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
}

}  // namespace edpbox
}  // namespace esphome
