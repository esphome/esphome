#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include <span>

namespace esphome::modbus_controller {

enum class RawEncoding { NONE = 0, HEXBYTES = 1, COMMA = 2, ANSI = 3 };

class ModbusTextSensor final : public Component, public text_sensor::TextSensor, public SensorItem {
 public:
  ModbusTextSensor(modbus::EntityType register_type, uint16_t start_address, uint8_t offset, uint8_t register_count,
                   uint16_t response_bytes, RawEncoding encode, uint16_t skip_updates, bool force_new_range) {
    this->register_type = register_type;
    this->set_address(start_address);
    this->set_offset_from_start_address(offset);
    this->response_bytes = response_bytes;
    this->register_count = register_count;
    this->encode_ = encode;
    this->skip_updates = skip_updates;
    this->bitmask = 0xFFFFFFFF;
    this->sensor_value_type = SensorValueType::RAW;
    this->force_new_range = force_new_range;
  }

  void dump_config() override;

  void parse_and_publish(std::span<const uint8_t> data) override;
  using transform_func_t = optional<std::string> (*)(ModbusTextSensor *, std::string, std::span<const uint8_t>);
  void set_template(transform_func_t f) { this->transform_func_ = f; }

 protected:
  optional<transform_func_t> transform_func_{nullopt};

  RawEncoding encode_;
};

}  // namespace esphome::modbus_controller
