#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#ifdef ARDUINO_ARCH_ESP32
#include <mbedtls/ccm.h>
#include <mbedtls/error.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <mbedtls/ccm.h>
#include <mbedtls/error.h>
#endif

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_ble {

struct XiaomiParseResult {
  enum { TYPE_LYWSDCGQ, TYPE_HHCCJCY01, TYPE_LYWSD02, TYPE_CGG1, TYPE_LYWSD03 } type;
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  optional<float> conductivity;
  optional<float> illuminance;
  optional<float> moisture;
  bool has_data = false;        // 0x40
  bool has_capability = false;  // 0x20
  bool has_encryption = false;  // 0x08
  int data_length = 0;
};

bool parse_xiaomi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, XiaomiParseResult &result);

optional<XiaomiParseResult> parse_xiaomi_header(const esp32_ble_tracker::ESPBTDevice &device);

void parse_xiaomi_message(const uint8_t *message, xiaomi_ble::XiaomiParseResult &result);

void decrypt_message(const esp32_ble_tracker::ESPBTDevice &device, xiaomi_ble::XiaomiParseResult &result,
                     const uint8_t *bindkey, uint8_t *message);

class XiaomiListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace xiaomi_ble
}  // namespace esphome

#endif
