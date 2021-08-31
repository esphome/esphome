#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

class ModbusSwitch : public Component, public switch_::Switch, public SensorItem {
 public:
  ModbusSwitch(ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask)
      : Component(),
        switch_::Switch(),
        register_type(register_type),
        start_address(start_address),
        offset(offset),
        bitmask(bitmask),
        sensor_value_type(SensorValueType::BIT),
        register_count(1),
        skip_updates(0) {
    if (register_type == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
        register_type == ModbusFunctionCode::WRITE_SINGLE_COIL) {
      this->start_address += offset;
      this->offset = 0;
    }
  };

  void write_state(bool state) override;
  void dump_config() override;

  void set_state(bool state) { this->state = state; }

  float parse_and_publish(const std::vector<uint8_t> &data) override;

  void set_parent(ModbusController *parent) { this->parent_ = parent; }

 protected:
  ModbusController *parent_;
};

}  // namespace modbus_controller
}  // namespace esphome
