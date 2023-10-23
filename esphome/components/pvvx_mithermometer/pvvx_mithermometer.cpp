#include "pvvx_mithermometer.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace pvvx_mithermometer {

static const char *const TAG = "pvvx_mithermometer";

void PVVXMiThermometer::dump_config() {
  ESP_LOGCONFIG(TAG, "PVVX MiThermometer");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
}

bool PVVXMiThermometer::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (this->signal_strength_ != nullptr)
      this->signal_strength_->publish_state(device.get_rssi());
    success = true;
  }

  return success;
}

optional<ParseResult> PVVXMiThermometer::parse_header_(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (!service_data.uuid.contains(0x1A, 0x18)) {
    ESP_LOGVV(TAG, "parse_header(): no service data UUID magic bytes.");
    return {};
  }

  auto raw = service_data.data;

  static uint8_t last_frame_count = 0;
  if (last_frame_count == raw[13]) {
    ESP_LOGVV(TAG, "parse_header(): duplicate data packet received (%hhu).", last_frame_count);
    return {};
  }
  last_frame_count = raw[13];

  return result;
}

bool PVVXMiThermometer::parse_message_(const std::vector<uint8_t> &message, ParseResult &result) {
  /*
  All data little endian
  uint8_t     size;   // = 19
  uint8_t     uid;    // = 0x16, 16-bit UUID
  uint16_t    UUID;   // = 0x181A, GATT Service 0x181A Environmental Sensing
  uint8_t     MAC[6]; // [0] - lo, .. [5] - hi digits
  int16_t     temperature;    // x 0.01 degree     [6,7]
  uint16_t    humidity;       // x 0.01 %          [8,9]
  uint16_t    battery_mv;     // mV                [10,11]
  uint8_t     battery_level;  // 0..100 %          [12]
  uint8_t     counter;        // measurement count [13]
  uint8_t     flags;  [14]
  */

  const uint8_t *data = message.data();
  const int data_length = 15;

  if (message.size() != data_length) {
    ESP_LOGVV(TAG, "parse_message(): payload has wrong size (%d)!", message.size());
    return false;
  }

  // int16_t     temperature;    // x 0.01 degree     [6,7]
  const int16_t temperature = int16_t(data[6]) | (int16_t(data[7]) << 8);
  result.temperature = temperature / 1.0e2f;

  // uint16_t    humidity;       // x 0.01 %          [8,9]
  const int16_t humidity = uint16_t(data[8]) | (uint16_t(data[9]) << 8);
  result.humidity = humidity / 1.0e2f;

  // uint16_t    battery_mv;     // mV                [10,11]
  const int16_t battery_voltage = uint16_t(data[10]) | (uint16_t(data[11]) << 8);
  result.battery_voltage = battery_voltage / 1.0e3f;

  // uint8_t     battery_level;  // 0..100 %          [12]
  result.battery_level = uint8_t(data[12]);

  return true;
}

bool PVVXMiThermometer::report_results_(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGD(TAG, "Got PVVX MiThermometer (%s):", address.c_str());

  if (result->temperature.has_value()) {
    ESP_LOGD(TAG, "  Temperature: %.2f °C", *result->temperature);
  }
  if (result->humidity.has_value()) {
    ESP_LOGD(TAG, "  Humidity: %.2f %%", *result->humidity);
  }
  if (result->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f %%", *result->battery_level);
  }
  if (result->battery_voltage.has_value()) {
    ESP_LOGD(TAG, "  Battery Voltage: %.3f V", *result->battery_voltage);
  }

  return true;
}

}  // namespace pvvx_mithermometer
}  // namespace esphome

#endif
