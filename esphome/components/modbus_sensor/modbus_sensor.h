#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome {
namespace modbus_sensor {

static const uint8_t READ_REGISTERS_FUNCTION = 0x04;

enum RegisterType { REGISTER_TYPE_16BIT, REGISTER_TYPE_32BIT, REGISTER_TYPE_32BIT_REVERSED };

struct Register {
  sensor::Sensor *sensor;
  RegisterType register_type;
};

class ModbusSensor : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_register_address(uint16_t register_address) { this->register_address_ = register_address; }
  void set_register_count(uint16_t register_count) { this->register_count_ = register_count; }
  void update() override { this->send(READ_REGISTERS_FUNCTION, register_address_, register_count_); }

  void set_sensor(sensor::Sensor *sensor, RegisterType register_type);
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;

 protected:
  std::vector<Register> registers_;
  std::vector<uint8_t> sensors_length_;
  std::vector<bool> sensors_reverse_order_;
  uint16_t register_address_;
  uint16_t register_count_ = 0;
};

}  // namespace modbus_sensor
}  // namespace esphome
