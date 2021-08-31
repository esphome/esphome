#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

class ModbusSensor : public Component, public sensor::Sensor, public SensorItem {
 public:
  ModbusSensor(ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               SensorValueType value_type, int register_count, uint8_t skip_updates)
      : Component(), sensor::Sensor() {
        this->register_type = register_type;
        this->start_address = start_address;
        this->offset = offset;
        this->bitmask = bitmask;
        this->sensor_value_type = value_type;
        this->register_count = register_count;
        this->skip_updates = skip_updates;
        }

  float parse_and_publish(const std::vector<uint8_t> &data) override;

  void dump_config() override;
};

}  // namespace modbus_controller
}  // namespace esphome
