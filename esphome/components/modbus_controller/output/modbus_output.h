#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"

namespace esphome {
namespace modbus_controller {

using value_to_data_t = std::function<float>(float);

class ModbusOutput : public output::FloatOutput, public Component, public SensorItem {
 public:
  ModbusOutput(uint16_t start_address, uint8_t offset, SensorValueType value_type)
      : output::FloatOutput(), Component() {
    this->register_type = ModbusRegisterType::HOLDING;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask;
    this->sensor_value_type = value_type;
    this->skip_updates = 0;
    this->start_address += offset;
    this->offset = 0;
  }
  void setup() override;
  void dump_config() override;

  void set_parent(ModbusController *parent) { this->parent_ = parent; }
  void set_write_multiply(float factor) { multiply_by_ = factor; }
  // Do nothing
  void parse_and_publish(const std::vector<uint8_t> &data) override{};

  using write_transform_func_t = std::function<optional<float>(ModbusOutput *, float, std::vector<uint16_t> &)>;
  void set_write_template(write_transform_func_t &&f) { this->write_transform_func_ = f; }

 protected:
  void write_state(float value) override;
  optional<write_transform_func_t> write_transform_func_{nullopt};

  ModbusController *parent_;
  float multiply_by_{1.0};
};

}  // namespace modbus_controller
}  // namespace esphome
