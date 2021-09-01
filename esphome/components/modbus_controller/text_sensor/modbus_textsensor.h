#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

enum class RawEncoding { NONE = 0, HEXBYTES = 1, COMMA = 2 };

class ModbusTextSensor : public Component, public text_sensor::TextSensor, public SensorItem {
 public:
  ModbusTextSensor(ModbusFunctionCode register_type, uint16_t start_address, uint8_t offset, uint8_t register_count,
                   uint16_t response_bytes, RawEncoding encode, uint8_t skip_updates)
      : Component() {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->response_bytes_ = response_bytes;
    this->register_count = register_count;
    this->encode_ = encode;
    this->skip_updates = skip_updates;
    this->bitmask = 0xFFFFFFFF;
    this->sensor_value_type = SensorValueType::RAW;
  }

  void dump_config() override;

  float parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  RawEncoding encode_;
  uint16_t response_bytes_;
};

}  // namespace modbus_controller
}  // namespace esphome
