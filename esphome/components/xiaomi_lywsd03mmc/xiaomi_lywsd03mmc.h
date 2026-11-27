#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_lywsd03mmc {

class XiaomiLYWSD03MMC final : public xiaomi_ble::XiaomiEncryptedDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(battery_level)

 public:
  void dump_child_config() override;

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;
};

}  // namespace esphome::xiaomi_lywsd03mmc
