#include "atc_mithermometer.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace atc_mithermometer {

static const char *const TAG = "atc_mithermometer";

void ATCMiThermometer::dump_config() {
  ESP_LOGCONFIG(TAG, "ATC MiThermometer");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
}

bool ATCMiThermometer::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    auto res = parse_header_(service_data);
    if (!res.has_value()) {
      continue;
    }
    if (!(parse_message_(service_data.data, *res))) {
      continue;
    }
    if (!(report_results_(res, device.address_str()))) {
      continue;
    }
    if (res->temperature.has_value() && this->temperature_ != nullptr)
      this->temperature_->publish_state(*res->temperature);
    if (res->humidity.has_value() && this->humidity_ != nullptr)
      this->humidity_->publish_state(*res->humidity);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    if (res->battery_voltage.has_value() && this->battery_voltage_ != nullptr)
      this->battery_voltage_->publish_state(*res->battery_voltage);
    success = true;
  }
  if (this->signal_strength_ != nullptr)
    this->signal_strength_->publish_state(device.get_rssi());

  return success;
}

optional<ParseResult> ATCMiThermometer::parse_header_(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x1A, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  auto raw = service_data.data;

  static uint8_t last_frame_count = 0;
  if (last_frame_count == raw[12]) {
    ESP_LOGVV(TAG, "parse_header(): duplicate data packet received (%hhu).", last_frame_count);
    return {};
  }
  last_frame_count = raw[12];

  return result;
}

bool ATCMiThermometer::parse_message(const std::vector<uint8_t> &message, ParseResult &result) {
  // Byte 0-5 mac in correct order
  // Byte 6-7 Temperature in uint16
  // Byte 8 Humidity in percent
  // Byte 9 Battery in percent
  // Byte 10-11 Battery in mV uint16_t
  // Byte 12 frame packet counter

  const uint8_t *data = message.data();
  const int data_length = 13;

  if (message.size() != data_length) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", message.size());
    return false;
  }

  // temperature, 2 bytes, 16-bit signed integer (LE), 0.1 °C
  const int16_t temperature = uint16_t(data[7]) | (uint16_t(data[6]) << 8);
  result.temperature = temperature / 10.0f;

  // humidity, 1 byte, 8-bit unsigned integer, 1.0 %
  result.humidity = data[8];

  // battery, 1 byte, 8-bit unsigned integer,  1.0 %
  result.battery_level = data[9];

  // battery, 2 bytes, 16-bit unsigned integer,  0.001 V
  const int16_t battery_voltage = uint16_t(data[11]) | (uint16_t(data[10]) << 8);
  result.battery_voltage = battery_voltage / 1.0e3f;

  return true;
}

bool ATCMiThermometer::report_results_(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got ATC MiThermometer (%s):", address.c_str());

  if (result->temperature.has_value()) {
    ESP_LOGD(TAG, "  Temperature: %.1f °C", *result->temperature);
  }
  if (result->humidity.has_value()) {
    ESP_LOGD(TAG, "  Humidity: %.0f %%", *result->humidity);
  }
  if (result->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f %%", *result->battery_level);
  }
  if (result->battery_voltage.has_value()) {
    ESP_LOGD(TAG, "  Battery Voltage: %.3f V", *result->battery_voltage);
  }

  return true;
}

}  // namespace atc_mithermometer
}  // namespace esphome

#endif
