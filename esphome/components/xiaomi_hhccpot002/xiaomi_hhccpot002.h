#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_hhccpot002 {

class XiaomiHHCCPOT002 final : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  void dump_config() override;
  void set_moisture(sensor::Sensor *moisture) { moisture_ = moisture; }
  void set_conductivity(sensor::Sensor *conductivity) { conductivity_ = conductivity; }

 protected:
  uint64_t address_;
  sensor::Sensor *moisture_{nullptr};
  sensor::Sensor *conductivity_{nullptr};
};

}  // namespace esphome::xiaomi_hhccpot002
