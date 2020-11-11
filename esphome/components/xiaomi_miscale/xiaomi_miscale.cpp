#include "xiaomi_miscale.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_miscale {

static const char *TAG = "xiaomi_miscale";

void XiaomiMiscale::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Miscale");
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance", this->impedance_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool XiaomiMiscale::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    if (res->impedance.has_value() && this->impedance_ != nullptr)
      this->impedance_->publish_state(*res->impedance);
    success = true;
  }

  if (!success) {
    return false;
  }

  return true;
}

optional<ParseResult> XiaomiMiscale::parse_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x1D, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  auto raw = service_data.data;

  static uint8_t last_frame_count = 0;
  if (last_frame_count == raw[12]) {
    ESP_LOGVV(TAG, "parse_header(): duplicate data packet received (%d).", static_cast<int>(last_frame_count));
    result.is_duplicate = true;
    return {};
  }
  last_frame_count = raw[12];
  result.is_duplicate = false;

  return result;
}

bool XiaomiMiscale::parse_message(const std::vector<uint8_t> &message, ParseResult &result) {

  const uint8_t *data = message.data();
  const int data_length = 10;

  if (message.size() != data_length) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", message.size());
    return false;
  }

  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  const uint16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
  if (data[0] == 0x22 || data[0] == 0xa2)
    result.weight = weight * 0.01f / 2.0f;
  else if (data[0] == 0x12 || data[0] == 0xb2)
    result.weight = weight * 0.01f * 0.6;
  else if (data[0] == 0x03 || data[0] == 0xb3)
    result.weight = weight * 0.01f * 0.453592;

  return true;
}

bool XiaomiMiscale::report_results(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got Xiaomi Miscale (%s):", address.c_str());

  if (result->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.1fkg", *result->weight);
  }
  if (result->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0f", *result->impedance);
  }
  if (result->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f %%", *result->battery_level);
  }

  return true;
}

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
