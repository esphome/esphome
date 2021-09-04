#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

class ModbusBinarySensor : public Component, public binary_sensor::BinarySensor, public SensorItem {
 public:
  ModbusBinarySensor(ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
                     uint8_t skip_updates)
      : Component(), binary_sensor::BinarySensor() {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask;
    this->sensor_value_type = SensorValueType::BIT;
    this->skip_updates = skip_updates;

    if (register_type == ModbusFunctionCode::READ_COILS || register_type == ModbusFunctionCode::READ_DISCRETE_INPUTS)
      this->register_count = offset + 1;
    else
      this->register_count = 1;
  }

  void parse_and_publish(const std::vector<uint8_t> &data) override;
  void set_state(bool state) { this->state = state; }

  void dump_config() override;
};

}  // namespace modbus_controller
}  // namespace esphome
