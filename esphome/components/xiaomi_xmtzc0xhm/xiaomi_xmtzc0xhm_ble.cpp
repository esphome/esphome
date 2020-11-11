#include "xiaomi_xmtzc0xhm_ble.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_xmtzc0xhm_ble {

static const char *TAG = "xiaomi_xmtzc0xhm_ble";

bool parse_xiaomi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, XiaomiParseResult &result) {
  switch (data_type) {
    case 0x16: {  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
      if (result.type == XiaomiParseResult::TYPE_XMTZC0XHM) {
        switch (data_length) {
          case 10: {
            const uint16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
            if (data[0] == 0x22 || data[0] == 0xa2)
              result.weight = weight * 0.01f / 2.0f;
            else if (data[0] == 0x12 || data[0] == 0xb2)
              result.weight = weight * 0.01f * 0.6;
            else if (data[0] == 0x03 || data[0] == 0xb3)
              result.weight = weight * 0.01f * 0.453592;
            else
              return false;

            return true;
          }
          case 13: {
            const uint16_t weight = uint16_t(data[11]) | (uint16_t(data[12]) << 8);
            const uint16_t impedance = uint16_t(data[9]) | (uint16_t(data[10]) << 8);
            result.impedance = impedance;

            if (data[0] == 0x02)
              result.weight = weight * 0.01f / 2.0f;
            else if (data[0] == 0x03)
              result.weight = weight * 0.01f * 0.453592;
            else
              return false;

            return true;
          }
        }
      }
    }
    default:
      return false;
  }
}

bool parse_xiaomi_service_data(XiaomiParseResult &result, const esp32_ble_tracker::ServiceData &service_data) {
  if (!service_data.uuid.contains(0x1D, 0x18) {
    ESP_LOGVV(TAG, "Xiaomi no service data UUID magic bytes");
    return false;
  }

  const auto raw = service_data.data;

  if (raw.size() < 13) {
    ESP_LOGVV(TAG, "Xiaomi service data too short!");
    return false;
  }

  bool is_xmtzc0xhm = service_data.uuid.contains(0x1D, 0x18);
  bool is_mibfs = service_data.uuid.contains(0x1B, 0x18);

  if (!is_xmtzc0xhm && !is_mibfs) {
    ESP_LOGVV(TAG, "Xiaomi no magic bytes");
    return false;
  }

  result.type = XiaomiParseResult::TYPE_HHCCJCY01;
  if (is_xmtzc0xhm) {
    result.type = XiaomiParseResult::TYPE_XMTZC0XHM;
  } else if (is_mibfs) {
    result.type = XiaomiParseResult::TYPE_XMTZC0XHM;
  }

  // Hack for MiScale
  if (is_xmtzc0xhm || is_mibfs) {
    const uint8_t *datapoint_data = &raw[0];  // raw data
    if (parse_xiaomi_data_byte(0x16, datapoint_data, raw.size(), result))
      success = true;
  }

  return success;
}
optional<XiaomiParseResult> parse_xiaomi(const esp32_ble_tracker::ESPBTDevice &device) {
  XiaomiParseResult result;
  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    if (parse_xiaomi_service_data(result, service_data))
      success = true;
  }
  if (!success)
    return {};
  return result;
}

bool XiaomiListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  auto res = parse_xiaomi(device);
  if (!res.has_value())
    return false;

  const char *name = "XMTZC0XHM";
  if (res->type == XiaomiParseResult::TYPE_XMTZC0XHM) {
    name = "XMTZC0XHM";
  }

  ESP_LOGD(TAG, "Got Xiaomi %s (%s):", name, device.address_str().c_str());

  if (res->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f%%", *res->battery_level);
  }
  if (res->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.1fkg", *res->weight);
  }
  if (res->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0f", *res->impedance);
  }

  return true;
}

}  // namespace xiaomi_xmtzc0xhm_ble
}  // namespace esphome

#endif
