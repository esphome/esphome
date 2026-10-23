#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_hhccpot002 {

class XiaomiHHCCPOT002 final : public Component, public xiaomi_ble::XiaomiDevice {
 public:
  void dump_config() override;
  void set_moisture(sensor::Sensor *moisture) { moisture_ = moisture; }
  void set_conductivity(sensor::Sensor *conductivity) { conductivity_ = conductivity; }

 protected:
  void publish_result_(const xiaomi_ble::XiaomiParseResult &res) override;

  sensor::Sensor *moisture_{nullptr};
  sensor::Sensor *conductivity_{nullptr};
};

}  // namespace esphome::xiaomi_hhccpot002
