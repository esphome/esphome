#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome {
namespace modbus_controller {

class ModbusFloatOutput : public output::FloatOutput, public Component, public SensorItem {
 public:
  ModbusFloatOutput(uint16_t start_address, uint8_t offset, SensorValueType value_type, int register_count) {
    this->register_type = ModbusRegisterType::HOLDING;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask;
    this->register_count = register_count;
    this->sensor_value_type = value_type;
    this->skip_updates = 0;
    this->start_address += offset;
    this->offset = 0;
  }
  void dump_config() override;

  void set_parent(ModbusController *parent) { this->parent_ = parent; }
  void set_write_multiply(float factor) { multiply_by_ = factor; }
  // Do nothing
  void parse_and_publish(const std::vector<uint8_t> &data) override{};

  using write_transform_func_t = std::function<optional<float>(ModbusFloatOutput *, float, std::vector<uint16_t> &)>;
  void set_write_template(write_transform_func_t &&f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  void write_state(float value) override;
  optional<write_transform_func_t> write_transform_func_{nullopt};

  ModbusController *parent_;
  float multiply_by_{1.0};
  bool use_write_multiple_;
};

class ModbusBinaryOutput : public output::BinaryOutput, public Component, public SensorItem {
 public:
  ModbusBinaryOutput(uint16_t start_address, uint8_t offset) {
    this->register_type = ModbusRegisterType::COIL;
    this->start_address = start_address;
    this->bitmask = bitmask;
    this->sensor_value_type = SensorValueType::BIT;
    this->skip_updates = 0;
    this->register_count = 1;
    this->start_address += offset;
    this->offset = 0;
  }
  void dump_config() override;

  void set_parent(ModbusController *parent) { this->parent_ = parent; }
  // Do nothing
  void parse_and_publish(const std::vector<uint8_t> &data) override{};

  using write_transform_func_t = std::function<optional<bool>(ModbusBinaryOutput *, bool, std::vector<uint8_t> &)>;
  void set_write_template(write_transform_func_t &&f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  void write_state(bool state) override;
  optional<write_transform_func_t> write_transform_func_{nullopt};

  ModbusController *parent_;
  bool use_write_multiple_;
};

}  // namespace modbus_controller
}  // namespace esphome
