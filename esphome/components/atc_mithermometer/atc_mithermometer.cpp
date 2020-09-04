#include "atc_mithermometer.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace atc_mithermometer {

static const char *TAG = "atc_mithermometer";

void ATCMiThermometer::dump_config() {
  ESP_LOGCONFIG(TAG, "ATC MiThermometer");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool ATCMiThermometer::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (res->temperature.has_value() && this->temperature_ != nullptr)
      this->temperature_->publish_state(*res->temperature);
    if (res->humidity.has_value() && this->humidity_ != nullptr)
      this->humidity_->publish_state(*res->humidity);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    success = true;
  }

  if (!success) {
    return false;
  }

  return true;
}

optional<ParseResult> ATCMiThermometer::parse_header(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x18, 0x1A)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  auto raw = service_data.data;

  static uint8_t last_frame_count = 0;
  if (last_frame_count == raw[16]) {
    ESP_LOGVV(TAG, "parse_xiaomi_header(): duplicate data packet received (%d).", static_cast<int>(last_frame_count));
    result.is_duplicate = true;
    return {};
  }
  last_frame_count = raw[16];
  result.is_duplicate = false;

  return result;
}

bool ATCMiThermometer::parse_message(const std::vector<uint8_t> &message, ParseResult &result) {

  // Byte 5-10 mac in correct order
  // Byte 11-12 Temperature in uint16
  // Byte 13 Humidity in percent
  // Byte 14 Battery in percent
  // Byte 15-16 Battery in mV uint16_t
  // Byte 17 frame packet counter

  const uint8_t *raw = message.data();
  const uint8_t *data = raw + 3;

  if (message.size() != 17) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", data_length);
    return false;
  }

  // temperature, 2 bytes, 16-bit signed integer (LE), 0.1 °C
  else if (raw[0] == 0x04) {
    const int16_t temperature = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    result.temperature = temperature / 10.0f;
  }
  // humidity, 2 bytes, 16-bit signed integer (LE), 0.1 %
  else if (raw[0] == 0x06) {
    const int16_t humidity = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
    result.humidity = humidity / 10.0f;
  }
  // battery, 1 byte, 8-bit unsigned integer, 1 %
  else if (raw[0] == 0x0A) {
    result.battery_level = data[0];

  } else {
    return false;
  }

  return true;
}

bool ATCMiThermometer::report_results(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got ATC MiThermometer (%s):", address.c_str());

  if (result->temperature.has_value()) {
    ESP_LOGD(TAG, "  Temperature: %.1f°C", *result->temperature);
  }
  if (result->humidity.has_value()) {
    ESP_LOGD(TAG, "  Humidity: %.1f%%", *result->humidity);
  }
  if (result->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f%%", *result->battery_level);
  }

  return true;
}

}  // namespace atc_mithermometer
}  // namespace esphome

#endif
