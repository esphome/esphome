#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome {
namespace modbus_sensor {

static const uint8_t READ_REGISTERS_FUNCTION = 0x04;

class ModbusSensor : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_sensor(sensor::Sensor *sensor) { sensors_.push_back(sensor); }
  void set_sensor_reverse_order(bool reverse_order) { sensors_reverse_order_.push_back(reverse_order); }
  void set_register(uint16_t register_address) { this->register_ = register_address; }
  void set_register_count(uint16_t register_count) { this->register_count_ = register_count; }
  void update() override { this->send(READ_REGISTERS_FUNCTION, register_, register_count_); }

  void set_sensor_length(uint8_t length);
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;

 protected:
  std::vector<sensor::Sensor *> sensors_;
  std::vector<uint8_t> sensors_length_;
  std::vector<bool> sensors_reverse_order_;
  uint16_t register_;
  uint16_t register_count_;
  uint16_t response_size_;
};

}  // namespace modbus_sensor
}  // namespace esphome
