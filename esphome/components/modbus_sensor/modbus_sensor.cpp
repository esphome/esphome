#include "modbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_sensor {

static const char *TAG = "modbus.sensor";

void ModbusSensor::set_sensor(sensor::Sensor *sensor, RegisterType register_type) {
  this->registers_.push_back({sensor, register_type});
  if (register_type == REGISTER_TYPE_16BIT) {
    this->register_count_ += 2;
  } else {
    this->register_count_ += 4;
  }
}

void ModbusSensor::on_modbus_data(const std::vector<uint8_t> &data) {
  // Skip if data size doesn't mach the expected size
  if (data.size() != this->register_count_) {
    ESP_LOGV(TAG, "Skip data - size %lu - expected size: %d", data.size(), this->register_count_);
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

  RegisterType register_type;
  uint16_t value;
  uint16_t start = 0;
  for (uint16_t i = 0; i < this->registers_.size(); i++) {
    register_type = this->registers_[i].register_type;
    if (register_type == REGISTER_TYPE_16BIT) {
      value = get_16bit_value(start);
    } else if (register_type == REGISTER_TYPE_32BIT) {
      value = get_32bit_value(start);
    } else {
      value = get_32bit_value_reverse(start);
    }
    this->registers_[i].sensor->publish_state(value);
    start += this->sensors_length_[i] * 2;
  }
}

void ModbusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ModbusSensor:");
  ESP_LOGCONFIG(TAG, "  Address: %d", this->address_);
  ESP_LOGCONFIG(TAG, "  Register address: %d", this->register_address_);
  const char *register_type;
  for (uint16_t i = 0; i < this->registers_.size(); i++) {
    if (this->registers_[i].register_type == REGISTER_TYPE_16BIT) {
      register_type = "16bit";
    } else if (this->registers_[i].register_type == REGISTER_TYPE_32BIT) {
      register_type = "32bit";
    } else if (this->registers_[i].register_type == REGISTER_TYPE_32BIT_REVERSED) {
      register_type = "32bit_reversed";
    } else {
          register_type = "";
    }
    ESP_LOGCONFIG(TAG, "  Sensor %d:", i);
    ESP_LOGCONFIG(TAG, "    Register type: %s", register_type);
    LOG_SENSOR("  ", "  Name: ", this->registers_[i].sensor);
  }
}

}  // namespace modbus_sensor
}  // namespace esphome
