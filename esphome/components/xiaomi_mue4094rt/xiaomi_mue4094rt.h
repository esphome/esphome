#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_mue4094rt {

class XiaomiMUE4094RT final : public Component,
                              public binary_sensor::BinarySensorInitiallyOff,
                              public xiaomi_ble::XiaomiDevice {
 public:
  void dump_config() override;
  void set_time(uint16_t timeout) { timeout_ = timeout; }

 protected:
  void publish_result_(const xiaomi_ble::XiaomiParseResult &res) override;

  uint16_t timeout_;
};

}  // namespace esphome::xiaomi_mue4094rt
