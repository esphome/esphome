#include "modbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_sensor {

static const char *TAG = "modbus_sensor";


void ModbusSensor::set_sensor_length(uint8_t length) {
  sensors_length_.push_back(length);
  this->response_size_ = 0;
  for (int i = 0 ; i < this->sensors_.size(); i++) {
    this->response_size_ += this->sensors_length_[i] * 2;
  }
}


void ModbusSensor::on_modbus_data(const std::vector<uint8_t> &data) {
  // Skip if data size doesn't mach the expected size
  if (data.size() != this->response_size_) {
    ESP_LOGV(TAG, "Skip data - size %d - expected size: %d", data.size(), this->response_size_);
    return;
  }

  auto get_16bit_value = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto get_32bit_value = [&](size_t i) -> uint32_t {
    return (uint32_t(get_16bit_value(i + 2)) << 16) | (uint32_t(get_16bit_value(i + 0)) << 0);
  };
  auto get_32bit_value_reverse = [&](size_t i) -> uint32_t {
    return (uint32_t(get_16bit_value(i + 0)) << 16) | (uint32_t(get_16bit_value(i + 2)) << 0);
  };

  uint16_t value;
  uint16_t start = 0;
  for (uint16_t i = 0 ; i < this->sensors_.size(); i++) {
    if ( this->sensors_length_[i] == 1 ) {
      value = get_16bit_value(start);
      this->sensors_[i]->publish_state(value);
    }
    else if ( this->sensors_length_[i] == 2 ) {
      if (this->sensors_reverse_order_[i]) {
        value = get_32bit_value_reverse(start);
      } else {
        value = get_32bit_value(start);
      }
      this->sensors_[i]->publish_state(value);
    }

    start += this->sensors_length_[i] * 2;
  }

}

void ModbusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ModbusSensor:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

}  // namespace modbus_sensor
}  // namespace esphome
