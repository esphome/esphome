#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace oralb_ble {

struct OralbParseResult {
  optional<uint8_t> state;
};

bool parse_oralb_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, OralbParseResult &result);

optional<OralbParseResult> parse_oralb(const esp32_ble_tracker::ESPBTDevice &device);

class OralbListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace oralb_ble
}  // namespace esphome

#endif
