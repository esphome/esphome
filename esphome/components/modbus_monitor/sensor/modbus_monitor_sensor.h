#pragma once

#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/components/modbus_monitor/modbus_monitor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus_monitor {

class ModbusMonitorSensor final : public sensor::Sensor, public MonitorItem {
 public:
  ModbusMonitorSensor(uint8_t address, uint16_t reg, modbus::helpers::SensorValueType value_type)
      : MonitorItem(address, reg, modbus::helpers::register_width_for(value_type)), value_type_(value_type) {}

  /// Decoded as modbus_controller decodes a sensor, with the shared helper: signed types reinterpret the
  /// bits, so a discharging battery reports a negative power rather than ~65000.
  void parse_and_publish(std::span<const uint8_t> data) override {
    auto number = modbus::helpers::payload_to_number(data, this->value_type_, 0, 0xFFFFFFFF);
    if (!number.has_value())
      return;
    this->publish_state(modbus::helpers::value_type_is_float(this->value_type_)
                            ? bit_cast<float>(static_cast<uint32_t>(*number))
                            : static_cast<float>(*number));
  }

 protected:
  modbus::helpers::SensorValueType value_type_;
};

}  // namespace esphome::modbus_monitor
