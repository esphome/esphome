#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ruuvi_ble/ruuvi_ble.h"

namespace esphome::ruuvitag {

class RuuviTag final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(humidity)
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  SUB_SENSOR(acceleration)
  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(tx_power)
  SUB_SENSOR(movement_counter)
  SUB_SENSOR(measurement_sequence_number)

 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
    if (device.address_uint64() != this->address_)
      return false;

    auto res = ruuvi_ble::parse_ruuvi(device);
    if (!res.has_value())
      return false;

    if (res->humidity.has_value() && this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(*res->humidity);
    if (res->temperature.has_value() && this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(*res->temperature);
    if (res->pressure.has_value() && this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(*res->pressure);
    if (res->acceleration.has_value() && this->acceleration_sensor_ != nullptr)
      this->acceleration_sensor_->publish_state(*res->acceleration);
    if (res->acceleration_x.has_value() && this->acceleration_x_sensor_ != nullptr)
      this->acceleration_x_sensor_->publish_state(*res->acceleration_x);
    if (res->acceleration_y.has_value() && this->acceleration_y_sensor_ != nullptr)
      this->acceleration_y_sensor_->publish_state(*res->acceleration_y);
    if (res->acceleration_z.has_value() && this->acceleration_z_sensor_ != nullptr)
      this->acceleration_z_sensor_->publish_state(*res->acceleration_z);
    if (res->battery_voltage.has_value() && this->battery_voltage_sensor_ != nullptr)
      this->battery_voltage_sensor_->publish_state(*res->battery_voltage);
    if (res->tx_power.has_value() && this->tx_power_sensor_ != nullptr)
      this->tx_power_sensor_->publish_state(*res->tx_power);
    if (res->movement_counter.has_value() && this->movement_counter_sensor_ != nullptr)
      this->movement_counter_sensor_->publish_state(*res->movement_counter);
    if (res->measurement_sequence_number.has_value() && this->measurement_sequence_number_sensor_ != nullptr)
      this->measurement_sequence_number_sensor_->publish_state(*res->measurement_sequence_number);
    return true;
  }

  void dump_config() override;

 protected:
  uint64_t address_;
};

}  // namespace esphome::ruuvitag
