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

    sensor::publish_optional(this->humidity_sensor_, res->humidity);
    sensor::publish_optional(this->temperature_sensor_, res->temperature);
    sensor::publish_optional(this->pressure_sensor_, res->pressure);
    sensor::publish_optional(this->acceleration_sensor_, res->acceleration);
    sensor::publish_optional(this->acceleration_x_sensor_, res->acceleration_x);
    sensor::publish_optional(this->acceleration_y_sensor_, res->acceleration_y);
    sensor::publish_optional(this->acceleration_z_sensor_, res->acceleration_z);
    sensor::publish_optional(this->battery_voltage_sensor_, res->battery_voltage);
    sensor::publish_optional(this->tx_power_sensor_, res->tx_power);
    sensor::publish_optional(this->movement_counter_sensor_, res->movement_counter);
    sensor::publish_optional(this->measurement_sequence_number_sensor_, res->measurement_sequence_number);
    return true;
  }

  void dump_config() override;

 protected:
  uint64_t address_;
};

}  // namespace esphome::ruuvitag
