#include "thermopro_ble.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::thermopro_ble {

// this size must be large enough to hold the largest data frame
// of all supported devices
static constexpr std::size_t MAX_DATA_SIZE = 24;

struct DeviceParserMapping {
  const char *prefix;
  DeviceParser parser;
};

static float tp96_battery(uint16_t voltage);

static optional<ParseResult> parse_tp972(const uint8_t *data, std::size_t data_size);
static optional<ParseResult> parse_tp96(const uint8_t *data, std::size_t data_size);
static optional<ParseResult> parse_tp3(const uint8_t *data, std::size_t data_size);

static const char *const TAG = "thermopro_ble";

static const struct DeviceParserMapping DEVICE_PARSER_MAP[] = {
    {"TP972", parse_tp972}, {"TP970", parse_tp96}, {"TP96", parse_tp96}, {"TP3", parse_tp3}};

void ThermoProBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "ThermoPro BLE");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "External temperature", this->external_temperature_);
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
  update_device_type_(device.get_name());
  if (this->device_parser_ == nullptr) {
    ESP_LOGVV(TAG, "parse_device(): invalid device type.");
    return false;
  }

  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  // publish signal strength
  float signal_strength = float(device.get_rssi());
  if (this->signal_strength_ != nullptr)
    this->signal_strength_->publish_state(signal_strength);

  bool success = false;
  for (auto &service_data : device.get_manufacturer_datas()) {
    // check maximum data size
    std::size_t data_size = service_data.data.size() + 2;
    if (data_size > MAX_DATA_SIZE) {
      ESP_LOGVV(TAG, "parse_device(): maximum data size exceeded!");
      continue;
    }

    // reconstruct whole record from 2 byte uuid and data
    esp_bt_uuid_t uuid = service_data.uuid.get_uuid();
    uint8_t data[MAX_DATA_SIZE] = {static_cast<uint8_t>(uuid.uuid.uuid16), static_cast<uint8_t>(uuid.uuid.uuid16 >> 8)};
    std::copy(service_data.data.begin(), service_data.data.end(), std::begin(data) + 2);

    // dispatch data to parser
    optional<ParseResult> result = this->device_parser_(data, data_size);
    if (!result.has_value()) {
      continue;
    }

    // publish sensor values
    if (result->temperature.has_value() && this->temperature_ != nullptr)
      this->temperature_->publish_state(*result->temperature);
    if (result->external_temperature.has_value() && this->external_temperature_ != nullptr)
      this->external_temperature_->publish_state(*result->external_temperature);
    if (result->humidity.has_value() && this->humidity_ != nullptr)
      this->humidity_->publish_state(*result->humidity);
    if (result->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*result->battery_level);

    success = true;
  }

  return success;
}

void ThermoProBLE::update_device_type_(const std::string &device_name) {
  // check for changed device name (should only happen on initial call)
  if (this->device_name_ == device_name) {
    return;
  }

  // remember device name
  this->device_name_ = device_name;

  // try to find device parser
  for (const auto &mapping : DEVICE_PARSER_MAP) {
    if (device_name.starts_with(mapping.prefix)) {
      this->device_parser_ = mapping.parser;
      return;
    }
  }

  // device type unknown
  this->device_parser_ = nullptr;
  ESP_LOGVV(TAG, "update_device_type_(): unknown device type %s.", device_name.c_str());
}

static inline uint16_t read_uint16(const uint8_t *data, std::size_t offset) {
  return static_cast<uint16_t>(data[offset + 0]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
}

static inline int16_t read_int16(const uint8_t *data, std::size_t offset) {
  return static_cast<int16_t>(read_uint16(data, offset));
}

static inline uint32_t read_uint32(const uint8_t *data, std::size_t offset) {
  return static_cast<uint32_t>(data[offset + 0]) | (static_cast<uint32_t>(data[offset + 1]) << 8) |
         (static_cast<uint32_t>(data[offset + 2]) << 16) | (static_cast<uint32_t>(data[offset + 3]) << 24);
}

// Battery calculation used with permission from:
// https://github.com/Bluetooth-Devices/thermopro-ble/blob/main/src/thermopro_ble/parser.py
//
// TP96x battery values appear to be a voltage reading, probably in millivolts.
// This means that calculating battery life from it is a non-linear function.
// Examining the curve, it looked fairly close to a curve from the tanh function.
// So, I created a script to use Tensorflow to optimize an equation in the format
// A*tanh(B*x+C)+D
// Where A,B,C,D are the variables to optimize for. This yielded the below function
static float tp96_battery(uint16_t voltage) {
  float level = 52.317286f * tanh(static_cast<float>(voltage) / 273.624277936f - 8.76485439394f) + 51.06925f;
  return std::max(0.0f, std::min(level, 100.0f));
}

static optional<ParseResult> parse_tp972(const uint8_t *data, std::size_t data_size) {
  if (data_size != 23) {
    ESP_LOGVV(TAG, "parse_tp972(): payload has wrong size of %d (!= 23)!", data_size);
    return {};
  }

  ParseResult result;

  // ambient temperature, 2 bytes, 16-bit unsigned integer, -54 °C offset
  result.external_temperature = static_cast<float>(read_uint16(data, 1)) - 54.0f;

  // battery level, 2 bytes, 16-bit unsigned integer, voltage (convert to percentage)
  result.battery_level = tp96_battery(read_uint16(data, 3));

  // internal temperature, 4 bytes, float, -54 °C offset
  result.temperature = static_cast<float>(read_uint32(data, 9)) - 54.0f;

  return result;
}

static optional<ParseResult> parse_tp96(const uint8_t *data, std::size_t data_size) {
  if (data_size != 7) {
    ESP_LOGVV(TAG, "parse_tp96(): payload has wrong size of %d (!= 7)!", data_size);
    return {};
  }

  ParseResult result;

  // internal temperature, 2 bytes, 16-bit unsigned integer, -30 °C offset
  result.temperature = static_cast<float>(read_uint16(data, 1)) - 30.0f;

  // battery level, 2 bytes, 16-bit unsigned integer, voltage (convert to percentage)
  result.battery_level = tp96_battery(read_uint16(data, 3));

  // ambient temperature, 2 bytes, 16-bit unsigned integer, -30 °C offset
  result.external_temperature = static_cast<float>(read_uint16(data, 5)) - 30.0f;

  return result;
}

static optional<ParseResult> parse_tp3(const uint8_t *data, std::size_t data_size) {
  if (data_size < 6) {
    ESP_LOGVV(TAG, "parse_tp3(): payload has wrong size of %d (< 6)!", data_size);
    return {};
  }

  ParseResult result;

  // temperature, 2 bytes, 16-bit signed integer, 0.1 °C
  result.temperature = static_cast<float>(read_int16(data, 1)) * 0.1f;

  // humidity, 1 byte, 8-bit unsigned integer, 1.0 %
  result.humidity = static_cast<float>(data[3]);

  // battery level, 2 bits (0-2)
  result.battery_level = static_cast<float>(data[4] & 0x3) * 50.0;

  return result;
}

}  // namespace esphome::thermopro_ble

#endif
