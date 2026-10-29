#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_wx08zm {

class XiaomiWX08ZM final : public Component,
                           public binary_sensor::BinarySensorInitiallyOff,
                           public xiaomi_ble::XiaomiDevice {
  SUB_SENSOR(tablet)
  SUB_SENSOR(battery_level)

 public:
  void dump_config() override;

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;
};

}  // namespace esphome::xiaomi_wx08zm
