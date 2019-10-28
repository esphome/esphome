#include "ruuvi_ble.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ruuvi_ble {

static const char *TAG = "ruuvi_ble";

bool parse_ruuvi_data_byte(uint8_t data_type, uint8_t data_length, const uint8_t *data, RuuviParseResult &result) {
  switch (data_type) {
    case 0x03: {  // RAWv1
      if (data_length != 16)
        return false;

      const float humidity = float(data[0]) * 0.5f;

      const uint8_t temperatureSign = uint8_t(data[1] >> 7 & 1);
      const int8_t temperatureBase = int8_t(data[1] & 0x7F);
      const float temperatureFraction = float(data[2]) / 100.0f;
      const float temperature = float(temperatureBase) + temperatureFraction;

      const uint8_t pressureHi = uint8_t(data[3] & 0xFF);
      const uint8_t pressureLo = uint8_t(data[4] & 0xFF);
      const float pressure = float((pressureHi * 256.0f + 50000.0f + pressureLo) / 100.0f);

      const uint8_t acceleration_xSign = uint8_t(data[5] >> 7 & 1);

      const float acceleration_x = float(int16_t((data[5] << 8) + (data[6])) / 1000.0f);
      const float acceleration_y = float(int16_t((data[7] << 8) + (data[8])) / 1000.0f);
      const float acceleration_z = float(int16_t((data[9] << 8) + (data[10])) / 1000.0f);

      const uint8_t battHi = uint8_t(data[11] & 0xFF);
      const uint8_t battLo = uint8_t(data[12] & 0xFF);
      const float battery_voltage = float((battHi * 256.0f + battLo) / 1000.0f);

      result.humidity = humidity;
      result.temperature = temperatureSign == 1 ? temperature * -1 : temperature;
      result.pressure = pressure;
      result.acceleration_x = acceleration_x;
      result.acceleration_y = acceleration_y;
      result.acceleration_z = acceleration_z;
      result.battery_voltage = battery_voltage;
      return true;
    }
    default:
      return false;
  }
}
optional<RuuviParseResult> parse_ruuvi(const esp32_ble_tracker::ESPBTDevice &device) {
  const auto *raw = reinterpret_cast<const uint8_t *>(device.get_manufacturer_data().data());

  bool is_ruuvi = raw[0] == 0x99 && raw[1] == 0x04;

  if (!is_ruuvi) {
    return {};
  }

  const uint8_t data_length = device.get_manufacturer_data().size();
  const uint8_t format = raw[2];
  const uint8_t *data = &raw[3];

  RuuviParseResult result;

  bool success = parse_ruuvi_data_byte(format, data_length, data, result);
  if (!success)
    return {};
  return result;
}

bool RuuviListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  auto res = parse_ruuvi(device);
  if (!res.has_value())
    return false;

  ESP_LOGD(TAG, "Got %s (%s):", "RuuviTag", device.address_str().c_str());

  if (res->humidity.has_value()) {
    ESP_LOGD(TAG, "  Humidity: %.1f%%", *res->humidity);
  }
  if (res->temperature.has_value()) {
    ESP_LOGD(TAG, "  Temperature: %.2f°C", *res->temperature);
  }
  if (res->pressure.has_value()) {
    ESP_LOGD(TAG, "  Pressure: %.2fhPa", *res->pressure);
  }
  if (res->acceleration_x.has_value()) {
    ESP_LOGD(TAG, "  Acceleration X: %.3fG", *res->acceleration_x);
  }
  if (res->acceleration_y.has_value()) {
    ESP_LOGD(TAG, "  Acceleration Y: %.3fG", *res->acceleration_y);
  }
  if (res->acceleration_z.has_value()) {
    ESP_LOGD(TAG, "  Acceleration Z: %.3fG", *res->acceleration_z);
  }
  if (res->battery_voltage.has_value()) {
    ESP_LOGD(TAG, "  Battery Voltage: %.3fV", *res->battery_voltage);
  }

  return true;
}

}  // namespace ruuvi_ble
}  // namespace esphome

#endif
