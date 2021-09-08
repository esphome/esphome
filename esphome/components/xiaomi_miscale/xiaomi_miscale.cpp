#include "xiaomi_miscale.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_miscale {

static const char *const TAG = "xiaomi_miscale";

void XiaomiMiscale::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Miscale");
  ESP_LOGCONFIG(TAG, "  Version: %d", this->version_);
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance", this->impedance_);
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
    if (!res.has_value()) {
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

  return success;
}

optional<ParseResult> XiaomiMiscale::parse_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  auto service_uiid_byte = (this->version_ == 1) ? 0x1D : 0x1B;
  if (!service_data.uuid.contains(service_uiid_byte, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  return result;
}

bool XiaomiMiscale::parse_message(const std::vector<uint8_t> &message, ParseResult &result) {
  if (this->version_ == 1) {
    return parse_message_V1(message, result);
  } else {
    return parse_message_V2(message, result);
  }
}

bool XiaomiMiscale::parse_message_V1(const std::vector<uint8_t> &message, ParseResult &result) {
  // example 1d18 a2 6036 e307 07 11 0f1f11
  // 1-2 Weight (MISCALE 181D)
  // 3-4 Years (MISCALE 181D)
  // 5 month (MISCALE 181D)
  // 6 day (MISCALE 181D)
  // 7 hour (MISCALE 181D)
  // 8 minute (MISCALE 181D)
  // 9 second (MISCALE 181D)

  const uint8_t *data = message.data();
  const int data_length = 10;

  if (message.size() != data_length) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", message.size());
    return false;
  }

  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  const int16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
  if (data[0] == 0x22 || data[0] == 0xa2)
    result.weight = weight * 0.01f / 2.0f;  // unit 'kg'
  else if (data[0] == 0x12 || data[0] == 0xb2)
    result.weight = weight * 0.01f * 0.6;  // unit 'jin'
  else if (data[0] == 0x03 || data[0] == 0xb3)
    result.weight = weight * 0.01f * 0.453592;  // unit 'lbs'

  return true;
}

bool XiaomiMiscale::parse_message_V2(const std::vector<uint8_t> &message, ParseResult &result) {
  // 2-3 Years (MISCALE 2 181B)
  // 4 month (MISCALE 2 181B)
  // 5 day (MISCALE 2 181B)
  // 6 hour (MISCALE 2 181B)
  // 7 minute (MISCALE 2 181B)
  // 8 second (MISCALE 2 181B)
  // 9-10 impedance (MISCALE 2 181B)
  // 11-12 weight (MISCALE 2 181B)

  const uint8_t *data = message.data();
  const int data_length = 13;

  if (message.size() != data_length) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", message.size());
    return false;
  }

  bool has_impedance = ((data[1] & (1 << 1)) != 0) ? true : false;
  bool is_stabilized = ((data[1] & (1 << 5)) != 0) ? true : false;
  bool load_removed = ((data[1] & (1 << 7)) != 0) ? true : false;

  if (!is_stabilized || load_removed) {
    return false;
  }

  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  const int16_t weight = uint16_t(data[11]) | (uint16_t(data[12]) << 8);
  if (data[0] == 0x02)
    result.weight = weight * 0.01f / 2.0f;  // unit 'kg'
  else if (data[0] == 0x03)
    result.weight = weight * 0.01f * 0.453592;  // unit 'lbs'

  if (has_impedance) {
    // impedance, 2 bytes, 16-bit
    const int16_t impedance = uint16_t(data[9]) | (uint16_t(data[10]) << 8);
    result.impedance = impedance;

    if (impedance == 0 || impedance >= 3000) {
      return false;
    }
  }

  return true;
}

bool XiaomiMiscale::report_results(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got Xiaomi Miscale (%s):", address.c_str());

  if (result->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.2fkg", *result->weight);
  }
  if (result->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0fohm", *result->impedance);
  }

  return true;
}

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
