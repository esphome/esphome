#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_ble {

struct XiaomiParseResult {
  enum { TYPE_LYWSDCGQ, TYPE_HHCCJCY01, TYPE_LYWSD02, TYPE_CGG1 } type;
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  optional<float> conductivity;
  optional<float> illuminance;
  optional<float> moisture;
};

bool parse_xiaomi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, XiaomiParseResult &result);

optional<XiaomiParseResult> parse_xiaomi(const esp32_ble_tracker::ESPBTDevice &device);

class XiaomiListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace xiaomi_ble
}  // namespace esphome

#endif
