#include "xiaomi_xmtzc0xhm.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_xmtzc0xhm {

static const char *TAG = "xiaomi_xmtzc0xhm";

bool parse_value(uint8_t value_type, const uint8_t *data, uint8_t value_length, ParseResult &result) {
  // Miscale weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  else if ((value_type == 0x16) && (value_length == 10)) {
    const uint16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
    if (data[0] == 0x22 || data[0] == 0xa2)
      result.weight = weight * 0.01f / 2.0f;
    else if (data[0] == 0x12 || data[0] == 0xb2)
      result.weight = weight * 0.01f * 0.6;
    else if (data[0] == 0x03 || data[0] == 0xb3)
      result.weight = weight * 0.01f * 0.453592;
  }
  // Miscale 2 weight, impedence, 2 bytes, 16-bit  unsigned integer, 1 kg
  else if ((value_type == 0x16) && (value_length == 13)) {
    const uint16_t weight = uint16_t(data[11]) | (uint16_t(data[12]) << 8);
    const uint16_t impedance = uint16_t(data[9]) | (uint16_t(data[10]) << 8);
    result.impedance = impedance;

    if (data[0] == 0x02)
      result.weight = weight * 0.01f / 2.0f;
    else if (data[0] == 0x03)
      result.weight = weight * 0.01f * 0.453592;
  } else {
    return false;
  }

  return true;
}

bool parse_message(const std::vector<uint8_t> &message, ParseResult &result) {
  result.has_encryption = (message[0] & 0x08) ? true : false;  // update encryption status
  if (result.has_encryption) {
    ESP_LOGVV(TAG, "parse_message(): payload is encrypted, stop reading message.");
    return false;
  }

  // Data point specs
  // Byte 0: type
  // Byte 1: fixed 0x10
  // Byte 2: length
  // Byte 3..3+len-1: data point value

  const uint8_t *payload = message.data() + result.raw_offset;
  uint8_t payload_length = message.size() - result.raw_offset;
  uint8_t payload_offset = 0;
  bool success = false;

  if (payload_length < 4) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", payload_length);
    return false;
  }

  while (payload_length > 0) {
    if (payload[payload_offset + 1] != 0x10) {
      ESP_LOGVV(TAG, "parse_message(): fixed byte not found, stop parsing residual data.");
      break;
    }

    const uint8_t value_length = payload[payload_offset + 2];
    if ((value_length < 1) || (value_length > 4) || (payload_length < (3 + value_length))) {
      ESP_LOGVV(TAG, "parse_message(): value has wrong size (%d)!", value_length);
      break;
    }

    const uint8_t value_type = payload[payload_offset + 0];
    const uint8_t *data = &payload[payload_offset + 3];

    if (parse_value(value_type, data, value_length, result))
      success = true;

    payload_length -= 3 + value_length;
    payload_offset += 3 + value_length;
  }

  return success;
}

optional<ParseResult> parse_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x1D, 0x18) && !service_data.uuid.contains(0x1B, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  bool is_xmtzc0xhm = service_data.uuid.contains(0x1D, 0x18);
  bool is_mibfs = service_data.uuid.contains(0x1B, 0x18);

  if (!is_xmtzc0xhm && !is_mibfs) {
    ESP_LOGVV(TAG, "Xiaomi no magic bytes");
    return false;
  }

  if (is_xmtzc0xhm) {  // Xiaomi Miscale
    success = true
  } else if (is_mibfs) {  // Xiaomi Miscale 2
    success = true
  } else {
    ESP_LOGVV(TAG, "parse_header(): unknown device, no magic bytes.");
    return {};
  }

  return result;
}

bool report_results(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got Xiaomi %s (%s):", result->name.c_str(), address.c_str());

  if (result->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.2fkg", *result->weight);
  }
  if (result->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0f", *result->impedance);
  }

  return true;
}

bool Listener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // Previously the message was parsed twice per packet, once by Listener::parse_device()
  // and then again by the respective device class's parse_device() function. Parsing the header
  // here and then for each device seems to be unneccessary and complicates the duplicate packet filtering.
  // Hence I disabled the call to parse_header() here and the message parsing is done entirely
  // in the respecive device instance. The Listener class is defined in __init__.py and I was not
  // able to remove it entirely.

  return false;  // with true it's not showing device scans
}

}  // namespace xiaomi_ble
}  // namespace esphome

#endif
