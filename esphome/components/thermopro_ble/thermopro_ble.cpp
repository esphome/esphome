#include "thermopro_ble.h"
#include <algorithm>
#include <cmath>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::thermopro_ble {

ESPHOME_LOG_TAG(TAG, "thermopro_ble");

// Largest record (company ID plus payload) of all supported devices
static constexpr size_t MAX_DATA_SIZE = 24;

struct ParseResult {
  float temperature{NAN};
  float external_temperature{NAN};
  float humidity{NAN};
  float battery_level{NAN};
};

using DeviceParser = bool (*)(const uint8_t *data, size_t data_size, ParseResult &result);

// Fitted A*tanh(B*x+C)+D to the TP96x battery voltage curve, used with permission from
// https://github.com/Bluetooth-Devices/thermopro-ble/blob/main/src/thermopro_ble/parser.py
static float tp96_battery(uint16_t voltage) {
  float level = 52.317286f * std::tanh(static_cast<float>(voltage) / 273.624277936f - 8.76485439394f) + 51.06925f;
  return clamp(level, 0.0f, 100.0f);
}

// TP97x temperatures are °F offset by 54
static float tp97_celsius(float raw) { return fahrenheit_to_celsius(raw - 54.0f); }

// Some probes append 6 bytes of the base station MAC to the 7-byte frame
static bool parse_tp96(const uint8_t *data, size_t data_size, ParseResult &result) {
  if (data_size != 7 && data_size != 13) {
    ESP_LOGVV(TAG, "Probe payload size %u not supported", static_cast<unsigned>(data_size));
    return false;
  }
  // Temperatures are offset by 30 °C
  result.temperature = static_cast<float>(encode_value_le<uint16_t>(data + 1)) - 30.0f;
  result.battery_level = tp96_battery(encode_value_le<uint16_t>(data + 3));
  result.external_temperature = static_cast<float>(encode_value_le<uint16_t>(data + 5)) - 30.0f;
  return true;
}

// TP97x also sends the TP96 frame
static bool parse_tp97(const uint8_t *data, size_t data_size, ParseResult &result) {
  if (data_size != 23)
    return parse_tp96(data, data_size, result);
  result.external_temperature = tp97_celsius(encode_value_le<uint16_t>(data + 1));
  result.battery_level = tp96_battery(encode_value_le<uint16_t>(data + 3));
  // Probe tip, a float
  result.temperature = tp97_celsius(bit_cast<float>(encode_value_le<uint32_t>(data + 5)));
  return true;
}

static bool parse_tp3(const uint8_t *data, size_t data_size, ParseResult &result) {
  // Battery bits: low, half, full, unknown
  static constexpr float BATTERY_LEVELS[] = {1.0f, 50.0f, 100.0f, NAN};
  if (data_size < 6) {
    ESP_LOGVV(TAG, "TP3 payload size %u < 6", static_cast<unsigned>(data_size));
    return false;
  }
  // Temperature and humidity all 0xFF: no reading
  if (data[1] != 0xFF || data[2] != 0xFF || data[3] != 0xFF) {
    result.temperature = static_cast<float>(encode_value_le<int16_t>(data + 1)) * 0.1f;
    result.humidity = static_cast<float>(data[3]);
  }
  result.battery_level = BATTERY_LEVELS[data[4] & 0x3];
  return true;
}

struct DeviceParserMapping {
  const char *prefix;
  DeviceParser parser;
};

static constexpr DeviceParserMapping DEVICE_PARSER_MAP[] = {
    {"TP972", parse_tp97}, {"TP970", parse_tp97}, {"TP96", parse_tp96}, {"TP3", parse_tp3}};

static DeviceParser find_device_parser(StringRef device_name) {
  for (const auto &mapping : DEVICE_PARSER_MAP) {
    if (device_name.starts_with(mapping.prefix))
      return mapping.parser;
  }
  return nullptr;
}

static void publish_if_set(sensor::Sensor *sensor, float value) {
  if (sensor != nullptr && !std::isnan(value))
    sensor->publish_state(value);
}

void ThermoProBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "ThermoPro BLE");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "External Temperature", this->external_temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Signal Strength", this->signal_strength_sensor_);
}

bool ThermoProBLE::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  DeviceParser parser = find_device_parser(device.get_name());
  if (parser == nullptr) {
    ESP_LOGVV(TAG, "Unknown device type %s", device.get_name().c_str());
    return false;
  }

  PUBLISH_STATE(this->signal_strength_sensor_, device.get_rssi());

  bool success = false;
  for (const auto &mfg_data : device.get_manufacturer_datas()) {
    size_t data_size = mfg_data.data.size() + 2;
    if (data_size > MAX_DATA_SIZE) {
      ESP_LOGVV(TAG, "Payload size %u > %u", static_cast<unsigned>(data_size), static_cast<unsigned>(MAX_DATA_SIZE));
      continue;
    }

    // The parsers index the whole record, company ID included
    uint16_t company_id = mfg_data.uuid.uuid16();
    uint8_t data[MAX_DATA_SIZE] = {static_cast<uint8_t>(company_id), static_cast<uint8_t>(company_id >> 8)};
    std::copy(mfg_data.data.begin(), mfg_data.data.end(), data + 2);

    ParseResult result;
    if (!parser(data, data_size, result))
      continue;

    publish_if_set(this->temperature_sensor_, result.temperature);
    publish_if_set(this->external_temperature_sensor_, result.external_temperature);
    publish_if_set(this->humidity_sensor_, result.humidity);
    publish_if_set(this->battery_level_sensor_, result.battery_level);
    success = true;
  }

  return success;
}

}  // namespace esphome::thermopro_ble
