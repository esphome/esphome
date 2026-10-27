#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_wx08zm {

class XiaomiWX08ZM final : public Component,
                           public binary_sensor::BinarySensorInitiallyOff,
                           public xiaomi_ble::XiaomiDevice {
 public:
  void dump_config() override;
  void set_tablet(sensor::Sensor *tablet) { tablet_ = tablet; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;

  sensor::Sensor *tablet_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace esphome::xiaomi_wx08zm
