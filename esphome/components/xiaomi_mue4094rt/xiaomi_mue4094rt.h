#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_mue4094rt {

class XiaomiMUE4094RT final : public Component,
                              public binary_sensor::BinarySensorInitiallyOff,
                              public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  void dump_config() override;
  void set_time(uint16_t timeout) { timeout_ = timeout; }

 protected:
  uint64_t address_;
  uint16_t timeout_;
};

}  // namespace esphome::xiaomi_mue4094rt
