#pragma once

#include <stdint.h>
#include "modbus_controller.h"

namespace esphome {
namespace modbus_controller {

class ModbusSwitch : public Component, public switch_::Switch, public SensorItem {
 public:
  ModbusSwitch(ModbusFunctionCode register_type, uint16_t address, uint8_t offset, uint32_t bitmask)
      : Component(), switch_::Switch() {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask;
  };
  void write_state(bool state) override;
  void set_state(bool state) { this->state = state; }
  void set_modbus_parent(ModbusController *parent) { this->parent_ = parent; }
  float parse_and_publish(const std::vector<uint8_t> &data) override;
  void log() override;
  std::string const &get_sensorname() override { return this->get_name(); };
  void add_to_controller(ModbusController *master, ModbusFunctionCode register_type, uint16_t start_address,
                         uint8_t offset, uint32_t bitmask);

 protected:
  ModbusController *parent_{nullptr};
};

}  // namespace modbus_controller
}  // namespace esphome
