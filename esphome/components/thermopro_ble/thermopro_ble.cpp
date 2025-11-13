#include "thermopro_ble.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace thermopro_ble {

struct DeviceParserMapping {
  const std::string prefix;
  DeviceParser parser;
};

static optional<ParseResult> parse_tp357s(const std::vector<uint8_t> &data);

static const char *const TAG = "thermopro_ble";

static const struct DeviceParserMapping DEVICE_PARSER_MAP[] = {{"TP357S ", parse_tp357s}, {"", nullptr}};

void ThermoProBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "ThermoPro BLE");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool ThermoProBLE::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // check for matching mac address
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }

  // check for valid device type
  update_device_type(device.get_name());
  if (this->device_parser_ == nullptr) {
    ESP_LOGVV(TAG, "parse_device(): invalid device type.");
    return false;
  }

  ESP_LOGVV(TAG, "parse_device(): MAC address %s found (device type %d).", device.address_str().c_str(),
            this->device_type_);

  // publish signal strength
  float signal_strength = float(device.get_rssi());
  ESP_LOGD(TAG, "  Signal Strength: %.0f dBm", signal_strength);
  if (this->signal_strength_ != nullptr)
    this->signal_strength_->publish_state(signal_strength);

  bool success = false;
  for (auto &service_data : device.get_manufacturer_datas()) {
    // reconstruct whole record from 2 byte uuid and data
    esp_bt_uuid_t uuid = service_data.uuid.get_uuid();
    std::vector<uint8_t> data = {static_cast<uint8_t>(uuid.uuid.uuid16 & 0xff),
                                 static_cast<uint8_t>(uuid.uuid.uuid16 >> 8)};
    data.insert(data.end(), service_data.data.begin(), service_data.data.end());

    // dispatch data to parser
    optional<ParseResult> result;
    if (this->device_parser_ != nullptr) {
      result = this->device_parser_(data);
    }
    if (!result.has_value()) {
      continue;
    }

    // publish sensor values
    if (result->temperature.has_value() && this->temperature_ != nullptr)
      this->temperature_->publish_state(*result->temperature);
    if (result->humidity.has_value() && this->humidity_ != nullptr)
      this->humidity_->publish_state(*result->humidity);
    if (result->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*result->battery_level);

    success = true;
  }

  return success;
}

void ThermoProBLE::update_device_type(const std::string &device_name) {
  // check for changes device name (should only happen on initial call)
  if (this->device_name_ == device_name) {
    return;
  }

  // remember device name
  this->device_name_ = device_name;

  // try to find device parser
  for (const DeviceParserMapping *mapping = DEVICE_PARSER_MAP; mapping->parser != nullptr; mapping++) {
    if (device_name.starts_with(mapping->prefix)) {
      this->device_parser_ = mapping->parser;
      return;
    }
  }

  // device type unknown
  this->device_parser_ = nullptr;
  ESP_LOGVV(TAG, "update_device_type(): unknown device type %s.", device_name.c_str());
}

static optional<ParseResult> parse_tp357s(const std::vector<uint8_t> &data) {
  if (data.size() != 7) {
    ESP_LOGVV(TAG, "parse_tp357(): payload has wrong size (%d)!", data.size());
    return {};
  }

  ParseResult result;

  // temperature, 2 bytes, 16-bit signed integer, 0.1 °C
  result.temperature = static_cast<float>(uint16_t(data[1]) | (uint16_t(data[2]) << 8)) * 0.1f;

  // humidity, 1 byte, 8-bit unsigned integer, 1.0 %
  result.humidity = static_cast<float>(data[3]);

  // battery level, 2 bits (0-2)
  result.battery_level = static_cast<float>(data[4] & 0x3) * 50.0;

  return result;
}

}  // namespace thermopro_ble
}  // namespace esphome

#endif
