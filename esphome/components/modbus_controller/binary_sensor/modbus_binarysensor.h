#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"

#include <span>

namespace esphome::modbus_controller {

class ModbusBinarySensor final : public Component, public binary_sensor::BinarySensor, public SensorItem {
 public:
  ModbusBinarySensor(modbus::EntityType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
                     bool force_new_range) {
    this->register_type = register_type;
    this->set_address(start_address);
    this->set_offset_from_start_address(offset);
    this->bitmask = bitmask;
    this->sensor_value_type = SensorValueType::BIT;
    this->force_new_range = force_new_range;

    if (modbus::helpers::is_entity_type_binary(register_type)) {
      this->register_count = offset + 1;
    } else {
      this->register_count = 1;
    }
  }

  void parse_and_publish(std::span<const uint8_t> data) override;
  void set_state(bool state) { this->state = state; }

  void dump_config() override;

  using transform_func_t = optional<bool> (*)(ModbusBinarySensor *, bool, std::span<const uint8_t>);
  void set_template(transform_func_t f) { this->transform_func_ = f; }

 protected:
  optional<transform_func_t> transform_func_{nullopt};
};

}  // namespace esphome::modbus_controller
