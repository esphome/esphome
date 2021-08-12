#pragma once

#include <stdint.h>
#include "modbus_controller.h"

namespace esphome {
namespace modbus_controller {

class ModbusSensor : public Component, public sensor::Sensor, public SensorItem {
 public:
  ModbusSensor(const std::string &name)
      : Component(),
        sensor::Sensor(name){

        };
  ModbusSensor(const std::string &name, ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset,
               uint32_t bitmask, SensorValueType value_type = SensorValueType::U_WORD, int register_count = 1,
               uint8_t skip_updates = 0)
      : Component(), sensor::Sensor(name) {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask, this->sensor_value_type = value_type;
    this->register_count = register_count;
    this->skip_updates = skip_updates;
  }

  void update(){};
  void set_modbus_parent(ModbusController *parent) { this->parent_ = parent; }
  float parse_and_publish(const std::vector<uint8_t> &data) override;

  void log() override;
  std::string const &get_sensorname() override { return this->get_name(); };

  void add_to_controller(ModbusController *master, ModbusFunctionCode register_type, uint16_t start_address,
                         uint8_t offset, uint32_t bitmask, SensorValueType value_type, int register_count,
                         uint8_t skip_updates);

 protected:
  ModbusController *parent_{nullptr};
};

}  // namespace modbus_controller
}  // namespace esphome
