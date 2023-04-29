#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/modbus_device/modbus_device.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome {
namespace modbus_device {

using value_to_data_t = std::function<float>(float);

class ModbusNumber : public number::Number, public Component, public modbus_device::SensorItem {
 public:
  ModbusNumber(modbus::ModbusRegisterType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               modbus::SensorValueType value_type, int register_count) {
    this->register_type = register_type;
    this->start_address = start_address;
    this->offset = offset;
    this->bitmask = bitmask;
    this->sensor_value_type = value_type;
    this->register_count = register_count;
  };

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_parent(ModbusDevice *parent) { this->parent_ = parent; }

  using transform_func_t = std::function<optional<float>(ModbusNumber *, float)>;
  using read_transform_func_t = std::function<optional<float>(ModbusNumber *, float, std::vector<uint16_t> &)>;
  void set_template(transform_func_t &&f) { this->transform_func_ = f; }
  void set_read_template(read_transform_func_t &&f) { this->read_transform_func_ = f; }
  void set_write_multiply(float factor) { multiply_by_ = factor; }

 protected:
  void control(float value) override;
  void add_values_to_payload(std::vector<uint16_t> &payload, ssize_t offset) override;
  optional<read_transform_func_t> read_transform_func_;
  optional<transform_func_t> transform_func_;
  ModbusDevice *parent_;
  float multiply_by_{1.0};
};

}  // namespace modbus_device
}  // namespace esphome
