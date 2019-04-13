#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

namespace esphome {
namespace xiaomi_ble {

struct XiaomiParseResult {
  enum {
    TYPE_MIJIA,
    TYPE_MIFLORA
  } type;
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  optional<float> conductivity;
  optional<float> illuminance;
  optional<float> moisture;
};

bool parse_xiaomi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, XiaomiParseResult &result);

optional<XiaomiParseResult> parse_xiaomi(const esp32_ble_tracker::ESPBTDevice &device);

class XiaomiListener : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  XiaomiListener(esp32_ble_tracker::ESP32BLETracker *parent);
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void setup() override;
};

}  // namespace xiaomi_ble
}  // namespace esphome
