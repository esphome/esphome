#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome::modbus_controller {

class ModbusSwitch final : public Component, public switch_::Switch, public SensorItem, public WriterEntity {
 public:
  ModbusSwitch(modbus::EntityType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               bool force_new_range) {
    this->register_type = register_type;
    this->set_address(start_address);
    this->set_offset_from_start_address(offset);
    this->bitmask = bitmask;
    this->sensor_value_type = SensorValueType::BIT;
    this->register_count = 1;
    // A holding byte offset folds into the address as whole registers (odd offsets are rejected at
    // validation: a 16-bit register write cannot target half a register); a coil offset is a coil count.
    if (register_type == modbus::EntityType::HOLDING) {
      this->set_address(start_address + offset / 2);
      this->set_offset_from_start_address(0);
    } else if (register_type == modbus::EntityType::COIL) {
      this->set_address(start_address + offset);
      this->set_offset_from_start_address(0);
    }
    this->force_new_range = force_new_range;
  };
  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;
  void set_assumed_state(bool assumed_state);
  void set_state(bool state) { this->state = state; }
  void parse_and_publish(std::span<const uint8_t> data) override;
  void set_parent(ModbusController *parent) { this->set_controller_(parent); }

  using transform_func_t = optional<bool> (*)(ModbusSwitch *, bool, std::span<const uint8_t>);
  using write_transform_func_t = optional<bool> (*)(ModbusSwitch *, bool, modbus::helpers::PduBuffer &);
  void set_template(transform_func_t f) { this->publish_transform_func_ = f; }
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  bool assumed_state() override;
  bool use_write_multiple_{false};
  optional<transform_func_t> publish_transform_func_{nullopt};
  optional<write_transform_func_t> write_transform_func_{nullopt};
  bool assumed_state_{false};
};

}  // namespace esphome::modbus_controller
