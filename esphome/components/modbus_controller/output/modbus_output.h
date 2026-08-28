#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome::modbus_controller {

class ModbusFloatOutput final : public output::FloatOutput, public Component, public SensorItem, public WriterEntity {
 public:
  ModbusFloatOutput(uint16_t start_address, uint8_t offset, SensorValueType value_type, int register_count) {
    this->register_type = modbus::EntityType::HOLDING;
    // A byte offset folds into the address as whole registers; odd offsets are rejected at validation.
    this->set_address(start_address + offset / 2);
    this->set_offset_from_start_address(0);
    this->bitmask = 0xFFFFFFFF;
    this->register_count = register_count;
    this->sensor_value_type = value_type;
  }
  void dump_config() override;

  void set_parent(ModbusController *parent) { this->set_controller_(parent); }
  void set_write_multiply(float factor) { this->multiply_by_ = factor; }
  // Do nothing
  void parse_and_publish(std::span<const uint8_t> data) override{};

  using write_transform_func_t = optional<float> (*)(ModbusFloatOutput *, float, modbus::RegisterValues &);
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  void write_state(float value) override;
  optional<write_transform_func_t> write_transform_func_{nullopt};

  float multiply_by_{1.0};
  bool use_write_multiple_{false};
};

class ModbusBinaryOutput final : public output::BinaryOutput, public Component, public SensorItem, public WriterEntity {
 public:
  ModbusBinaryOutput(uint16_t start_address, uint8_t offset) {
    this->register_type = modbus::EntityType::COIL;
    // A coil offset is a coil count; fold it into the address.
    this->set_address(start_address + offset);
    this->bitmask = 0xFFFFFFFF;
    this->sensor_value_type = SensorValueType::BIT;
    this->register_count = 1;
    this->set_offset_from_start_address(0);
  }
  void dump_config() override;

  void set_parent(ModbusController *parent) { this->set_controller_(parent); }
  // Do nothing
  void parse_and_publish(std::span<const uint8_t> data) override{};

  using write_transform_func_t = optional<bool> (*)(ModbusBinaryOutput *, bool, modbus::helpers::PduBuffer &);
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  void write_state(bool state) override;
  optional<write_transform_func_t> write_transform_func_{nullopt};

  bool use_write_multiple_{false};
};

}  // namespace esphome::modbus_controller
