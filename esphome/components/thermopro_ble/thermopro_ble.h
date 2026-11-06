#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::thermopro_ble {

class ThermoProBLE final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(signal_strength)
  SUB_SENSOR(temperature)
  SUB_SENSOR(external_temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(battery_level)

 public:
  explicit ThermoProBLE(uint64_t address) : address_(address) {}

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  const uint64_t address_;
};

}  // namespace esphome::thermopro_ble
