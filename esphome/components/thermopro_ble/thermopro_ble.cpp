#include "thermopro_ble.h"
#include <cmath>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

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

ESPHOME_LOG_TAG(TAG, "thermopro_ble");

static const struct DeviceParserMapping DEVICE_PARSER_MAP[] = {
    {"TP972", parse_tp972}, {"TP970", parse_tp96}, {"TP96", parse_tp96}, {"TP3", parse_tp3}};

static DeviceParser find_device_parser(StringRef device_name) {
  for (const auto &mapping : DEVICE_PARSER_MAP) {
    if (device_name.starts_with(mapping.prefix))
      return mapping.parser;
  }
  ESP_LOGVV(TAG, "find_device_parser(): unknown device type %s.", device_name.c_str());
  return nullptr;
}

void ThermoProBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "ThermoPro BLE");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "External temperature", this->external_temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

bool ThermoProBLE::parse_device(const ble_device_base::ESPBTDevice &device) {
  // check for matching mac address
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }

  // check for valid device type
  DeviceParser parser = find_device_parser(device.get_name());
  if (parser == nullptr) {
    ESP_LOGVV(TAG, "parse_device(): invalid device type.");
    return false;
  }

  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str_to(addr_buf));

  // publish signal strength
  float signal_strength = float(device.get_rssi());
  PUBLISH_STATE(this->signal_strength_sensor_, signal_strength);

  bool success = false;
  for (auto &service_data : device.get_manufacturer_datas()) {
    // check maximum data size
    std::size_t data_size = service_data.data.size() + 2;
    if (data_size > MAX_DATA_SIZE) {
      ESP_LOGVV(TAG, "parse_device(): maximum data size exceeded!");
      continue;
    }

    // reconstruct whole record from 2 byte uuid and data
    uint16_t svc_uuid16 = service_data.uuid.uuid16();
    uint8_t data[MAX_DATA_SIZE] = {static_cast<uint8_t>(svc_uuid16), static_cast<uint8_t>(svc_uuid16 >> 8)};
    std::copy(service_data.data.begin(), service_data.data.end(), std::begin(data) + 2);

    // dispatch data to parser
    optional<ParseResult> result = parser(data, data_size);
    if (!result.has_value()) {
      continue;
    }

    // publish sensor values
    if (result->temperature.has_value() && this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(*result->temperature);
    if (result->external_temperature.has_value() && this->external_temperature_sensor_ != nullptr)
      this->external_temperature_sensor_->publish_state(*result->external_temperature);
    if (result->humidity.has_value() && this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(*result->humidity);
    if (result->battery_level.has_value() && this->battery_level_sensor_ != nullptr)
      this->battery_level_sensor_->publish_state(*result->battery_level);

    success = true;
  }

  return success;
}

static inline uint16_t read_uint16(const uint8_t *data, std::size_t offset) {
  return encode_value_le<uint16_t>(&data[offset]);
}

static inline int16_t read_int16(const uint8_t *data, std::size_t offset) {
  return encode_value_le<int16_t>(&data[offset]);
}

static inline uint32_t read_uint32(const uint8_t *data, std::size_t offset) {
  return encode_value_le<uint32_t>(&data[offset]);
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
  float level = 52.317286f * std::tanh(static_cast<float>(voltage) / 273.624277936f - 8.76485439394f) + 51.06925f;
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
  result.battery_level = static_cast<float>(data[4] & 0x3) * 50.0f;

  return result;
}

}  // namespace esphome::thermopro_ble
