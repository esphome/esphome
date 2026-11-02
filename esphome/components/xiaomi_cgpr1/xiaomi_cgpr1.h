#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_cgpr1 {

class XiaomiCGPR1 final : public Component,
                          public binary_sensor::BinarySensorInitiallyOff,
                          public xiaomi_ble::XiaomiEncryptedDevice {
  SUB_SENSOR(battery_level)
  SUB_SENSOR(illuminance)
  SUB_SENSOR(idle_time)

 public:
  void dump_config() override;

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;
};

}  // namespace esphome::xiaomi_cgpr1
