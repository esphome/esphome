#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::inkbird_ibsth1_mini {

class InkbirdIbstH1Mini final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(temperature)
  SUB_SENSOR(external_temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(battery_level)

 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  void dump_config() override;

 protected:
  uint64_t address_;
};

}  // namespace esphome::inkbird_ibsth1_mini
