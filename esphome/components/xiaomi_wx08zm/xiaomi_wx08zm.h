#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_wx08zm {

class XiaomiWX08ZM final : public Component,
                           public binary_sensor::BinarySensorInitiallyOff,
                           public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  void dump_config() override;
  void set_tablet(sensor::Sensor *tablet) { tablet_ = tablet; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }

 protected:
  uint64_t address_;
  sensor::Sensor *tablet_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace esphome::xiaomi_wx08zm
