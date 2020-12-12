#include "xiaomi_xmtzc0xhm.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_xmtzc0xhm {

static const char *TAG = "xiaomi_xmtzc0xhm";
void XiaomiXMTZC0XHM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi XMTZC0XHM");
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance", this->impedance_);
}

bool XiaomiXMTZC0XHM::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    auto res = XiaomiXMTZC0XHM::parse_header(service_data);
    if (!res.has_value()) {
      continue;
    }
    if (res->is_duplicate) {
      continue;
    }
    if (!(XiaomiXMTZC0XHM::parse_message(service_data.data, *res))) {
      continue;
    }
    if (!(XiaomiXMTZC0XHM::report_results(res, device.address_str()))) {
      continue;
    }
    if (res->weight.has_value() && this->weight_ != nullptr)
      this->weight_->publish_state(*res->weight);
    if (res->impedance.has_value() && this->impedance_ != nullptr)
      this->impedance_->publish_state(*res->impedance);
    success = true;
  }

  if (!success) {
    return false;
  }

  return true;
}


bool XiaomiXMTZC0XHM::parse_value(uint8_t value_type, const uint8_t *data, uint8_t value_length, ParseResult &result) {
  // Miscale weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  if ((value_type == 0x16) && (value_length == 10)) {
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

bool XiaomiXMTZC0XHM::parse_message(const std::vector<uint8_t> &message, ParseResult &result) {

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

optional<ParseResult> XiaomiXMTZC0XHM::parse_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x1D, 0x18) && !service_data.uuid.contains(0x1B, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  bool is_xmtzc0xhm = service_data.uuid.contains(0x1D, 0x18);
  bool is_mibfs = service_data.uuid.contains(0x1B, 0x18);

  if (is_xmtzc0xhm || is_mibfs) {
    ESP_LOGVV(TAG, "Xiaomi no magic bytes");
    return {};
  }

  return result;
}

bool XiaomiXMTZC0XHM::report_results(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got Xiaomi XMTZC0XHM (%s):", address.c_str());

  if (result->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.2fkg", *result->weight);
  }
  if (result->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0f", *result->impedance);
  }

  return true;
}

}  // namespace xiaomi_xmtzc0xhm
}  // namespace esphome

#endif
