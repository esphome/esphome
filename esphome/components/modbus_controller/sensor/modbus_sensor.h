#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <span>

namespace esphome::modbus_controller {

class ModbusSensor final : public Component, public sensor::Sensor, public SensorItem {
 public:
  ModbusSensor(modbus::EntityType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               SensorValueType value_type, int register_count, uint16_t skip_updates, bool force_new_range) {
    this->register_type = register_type;
    this->set_address(start_address);
    this->set_offset_from_start_address(offset);
    this->bitmask = bitmask;
    this->sensor_value_type = value_type;
    this->register_count = register_count;
    this->skip_updates = skip_updates;
    this->force_new_range = force_new_range;
  }

  void parse_and_publish(std::span<const uint8_t> data) override;
  void dump_config() override;
  using transform_func_t = optional<float> (*)(ModbusSensor *, float, std::span<const uint8_t>);

  void set_template(transform_func_t f) { this->transform_func_ = f; }

 protected:
  optional<transform_func_t> transform_func_{nullopt};
};

}  // namespace esphome::modbus_controller
