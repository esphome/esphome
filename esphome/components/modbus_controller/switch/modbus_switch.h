#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome::modbus_controller {

class ModbusSwitch final : public Component, public switch_::Switch, public SensorItem {
 public:
  ModbusSwitch(modbus::EntityType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               uint16_t skip_updates, bool force_new_range) {
    this->register_type = register_type;
    this->set_address(start_address);
    this->set_offset_from_start_address(offset);
    this->bitmask = bitmask;
    this->sensor_value_type = SensorValueType::BIT;
    this->skip_updates = skip_updates;
    this->register_count = 1;
    if (register_type == modbus::EntityType::HOLDING || register_type == modbus::EntityType::COIL) {
      this->set_address(this->start_address + offset);
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
  void set_parent(ModbusController *parent) { this->parent_ = parent; }

  using transform_func_t = optional<bool> (*)(ModbusSwitch *, bool, std::span<const uint8_t>);
  using write_transform_func_t = optional<bool> (*)(ModbusSwitch *, bool, std::vector<uint8_t> &);
  void set_template(transform_func_t f) { this->publish_transform_func_ = f; }
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  bool assumed_state() override;
  ModbusController *parent_{nullptr};
  bool use_write_multiple_{false};
  optional<transform_func_t> publish_transform_func_{nullopt};
  optional<write_transform_func_t> write_transform_func_{nullopt};
  bool assumed_state_{false};
};

}  // namespace esphome::modbus_controller
