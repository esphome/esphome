#pragma once

#include <stdint.h>
#include "modbus_controller.h"

namespace esphome {
namespace modbus_controller {

enum class RawEncoding { NONE = 0, HEXBYTES = 1, COMMA = 2 };

class ModbusTextSensor : public Component, public text_sensor::TextSensor, public SensorItem {
 public:
  ModbusTextSensor(const std::string &name, ModbusFunctionCode register_type, uint16_t address, uint8_t offset,
                   uint8_t register_count, uint16_t response_bytes, RawEncoding encode, uint8_t skip_updates)
      : Component(), text_sensor::TextSensor(name) {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;

    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->response_bytes_ = response_bytes;
    this->register_count = register_count;
    this->encode = encode;
    this->skip_updates = skip_updates;
  };

  ModbusTextSensor(const std::string &name) : text_sensor::TextSensor(name) {}
  ModbusTextSensor(ModbusFunctionCode register_type, uint16_t address, uint8_t offset, uint8_t register_count,
                   uint16_t response_bytes, RawEncoding encode, uint8_t skip_updates)
      : ModbusTextSensor("", register_type, address, offset, register_count, response_bytes, encode, skip_updates) {}
  float parse_and_publish(const std::vector<uint8_t> &data) override;
  void log() override;
  void add_to_controller(ModbusController *master, ModbusFunctionCode register_type, uint16_t start_address,
                         uint8_t offset, uint8_t register_count, uint16_t response_bytes, RawEncoding encode,
                         uint8_t skip_updates);
  std::string const &get_sensorname() override { return this->get_name(); };
  void update(){};
  void set_modbus_parent(ModbusController *parent) { this->parent_ = parent; }
  uint16_t response_bytes_;
  RawEncoding encode;

 protected:
  ModbusController *parent_{nullptr};
};

}  // namespace modbus_controller
}  // namespace esphome
