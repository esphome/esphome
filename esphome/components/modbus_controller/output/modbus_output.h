#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/modbus_controller/sensor/modbus_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace modbus_controller {

using value_to_data_t = std::function<float>(float);

class ModbusOutput : public output::FloatOutput, public Component, public SensorItem {
 public:
  ModbusOutput(uint16_t start_address, uint8_t offset, SensorValueType value_type)
      : output::FloatOutput(), Component() {
    this->register_type = ModbusFunctionCode::READ_HOLDING_REGISTERS;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask;
    this->sensor_value_type = value_type;
    this->skip_updates = 0;
    this->start_address += offset;
    this->offset = 0;
  }

  // void set_template(value_to_data_t &&lambda) { this->value_to_data_func_ = lambda; }
  void set_template(std::function<optional<float>(float, std::vector<uint16_t> &)> &&f) { this->transform_func_ = f; }
  void setup() override;
  void dump_config() override;

  void set_parent(ModbusController *parent) { this->parent_ = parent; }
  void set_multiply(float factor) { multiply_by_ = factor; }

  // Do nothing
  void parse_and_publish(const std::vector<uint8_t> &data) override{};

 protected:
  void write_state(float value) override;
  optional<std::function<optional<float>(float, std::vector<uint16_t> &)>> transform_func_;
  ModbusController *parent_;
  float multiply_by_{1.0};
};

}  // namespace modbus_controller
}  // namespace esphome
