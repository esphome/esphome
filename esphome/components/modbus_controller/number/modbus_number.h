#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/modbus_controller/sensor/modbus_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace modbus_controller {

using value_to_data_t = std::function<float>(float);

class ModbusNumber : public number::Number, public PollingComponent {
 public:
  ModbusNumber() : PollingComponent(), number::Number(){};

  // void set_template(value_to_data_t &&lambda) { this->value_to_data_func_ = lambda; }
  void set_template(std::function<optional<float>(float, std::vector<uint16_t> &)> &&f) { this->transform_func_ = f; }
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_parent(ModbusController *parent) { this->parent_ = parent; }
  void set_sensor(ModbusSensor *sensor) { this->connected_sensor_ = sensor; }
  void set_multiply(float factor) { multiply_by_ = factor; }

 protected:
  void control(float value) override;
  optional<std::function<optional<float>(float, std::vector<uint16_t> &)>> transform_func_;
  ModbusSensor *connected_sensor_{nullptr};
  ModbusController *parent_;
  float multiply_by_{1.0};
};

}  // namespace modbus_controller
}  // namespace esphome
