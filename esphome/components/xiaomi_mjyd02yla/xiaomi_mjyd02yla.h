#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_mjyd02yla {

class XiaomiMJYD02YLA final : public Component,
                              public binary_sensor::BinarySensorInitiallyOff,
                              public xiaomi_ble::XiaomiEncryptedDevice {
  SUB_SENSOR(idle_time)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(illuminance)
  SUB_BINARY_SENSOR(light)

 public:
  void dump_config() override;

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;
};

}  // namespace esphome::xiaomi_mjyd02yla
