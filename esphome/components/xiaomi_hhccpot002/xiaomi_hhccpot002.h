#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_hhccpot002 {

class XiaomiHHCCPOT002 final : public Component, public xiaomi_ble::XiaomiDevice {
  SUB_SENSOR(moisture)
  SUB_SENSOR(conductivity)

 public:
  void dump_config() override;

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;
};

}  // namespace esphome::xiaomi_hhccpot002
