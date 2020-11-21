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
    auto res = parse_header(service_data);
    if (res->is_duplicate) {
      continue;
    }
    if (!(parse_message(service_data.data, *res))) {
      continue;
    }
    if (!(report_results(res, device.address_str()))) {
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

optional<ParseResult> XiaomiXMTZC0XHM::parse_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x1B, 0x18) && !service_data.uuid.contains(0x1B, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  return result;
}

bool XiaomiXMTZC0XHM::parse_message(const std::vector<uint8_t> &message, ParseResult &result) {
  // 2-3 Weight (MISCALE 181D) // 2-3 Years (MISCALE 2 181B)
  // 4-5 Years (MISCALE 181D)  // 4 month (MISCALE 2 181B)
  //                           // 5 day (MISCALE 2 181B)
  // 6 month (MISCALE 181D)    // 6 hour (MISCALE 2 181B)
  // 7 day (MISCALE 181D)      // 7 minute (MISCALE 2 181B)
  // 8 hour (MISCALE 181D)     // 8 second (MISCALE 2 181B)
  // 9 minute (MISCALE 181D)   // 9-10 impedance (MISCALE 2 181B)
  // 10 second (MISCALE 181D)  //
  //                           // 11-12 weight (MISCALE 2 181B)

  const uint8_t *data = message.data();
  const int data_length = 10;
  const int data_length1 = data_length + 3;

  if (message.size() == data_length1) {
    // Miscale2 impedance, 2 bytes, 16-bit
    const int16_t impedance = uint16_t(data[9]) | (uint16_t(data[10]) << 8);
    result.impedance = impedance;

    // Miscale2 weight, 2 bytes, 16-bit  unsigned integer, 1 kg
    const int16_t weight = uint16_t(data[11]) | (uint16_t(data[12]) << 8);
    if (data[0] == 0x02)
      result.weight = weight * 0.01f / 2.0f;  // unit 'kg'
    else if (data[0] == 0x03)
      result.weight = weight * 0.01f * 0.453592;  // unit 'lbs'
  }

  else if (message.size() == data_length) {
    // Miscale weight, 2 bytes, 16-bit  unsigned integer, 1 kg
    const int16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
    if (data[0] == 0x22 || data[0] == 0xa2)
      result.weight = weight * 0.01f / 2.0f;  // unit 'kg'
    else if (data[0] == 0x12 || data[0] == 0xb2)
      result.weight = weight * 0.01f * 0.6;  // unit 'jin'
    else if (data[0] == 0x03 || data[0] == 0xb3)
      result.weight = weight * 0.01f * 0.453592;  // unit 'lbs'
  } else {
    return false;
  }

  return true;
}

bool XiaomiXMTZC0XHM::report_results(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got Xiaomi XMTZC0XHM (%s):", address.c_str());

  if (result->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.1fkg", *result->weight);
  }
  if (result->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0f", *result->impedance);
  }

  return true;
}

}  // namespace xiaomi_xmtzc0xhm
}  // namespace esphome

#endif
