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
  ModbusNumber() : number::Number(), PollingComponent(){};

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_parent(ModbusController *parent) { this->parent_ = parent; }
  void set_sensor(ModbusSensor *sensor) { this->connected_sensor_ = sensor; }
  void set_multiply(float factor) { multiply_by_ = factor; }

  using transform_func_t = std::function<optional<float>(float, std::vector<uint16_t> &)>;
  void set_template(transform_func_t &&f) { this->transform_func_ = f; }

 protected:
  void control(float value) override;
  optional<transform_func_t> transform_func_;
  ModbusSensor *connected_sensor_{nullptr};
  ModbusController *parent_;
  float multiply_by_{1.0};
};

}  // namespace modbus_controller
}  // namespace esphome
