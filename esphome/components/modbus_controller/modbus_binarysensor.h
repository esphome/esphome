#pragma once

#include <stdint.h>
#include "modbus_controller.h"

namespace esphome {
namespace modbus_controller {

class ModbusBinarySensor : public Component, public binary_sensor::BinarySensor, public SensorItem {
 public:
  ModbusBinarySensor(const std::string &name) : binary_sensor::BinarySensor(name) {}
  ModbusBinarySensor(const std::string &name, ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset,
                     uint32_t bitmask, uint8_t skip_updates = 0)
      : Component(), binary_sensor::BinarySensor(name) {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask, this->sensor_value_type = SensorValueType::BIT;
    this->skip_updates = skip_updates;
  }
  float parse_and_publish(const std::vector<uint8_t> &data) override;
  std::string const &get_sensorname() override { return this->get_name(); };
  void log() override;
  void update(){};
  void set_state(bool state) { this->state = state; }
  void set_modbus_parent(ModbusController *parent) { this->parent_ = parent; }
  void add_to_controller(ModbusController *master, ModbusFunctionCode register_type, uint16_t start_address,
                         uint8_t offset, uint32_t bitmask, uint8_t skip_updates = 0);

 protected:
  ModbusController *parent_{nullptr};
};

}  // namespace modbus_controller
}  // namespace esphome
