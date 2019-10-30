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

      const float humidity = uint8_t(data[0]) * 0.5f;
      const float temperature = int8_t(data[1]) + (uint8_t(data[2]) / 100.0f);
      const float pressure = (uint16_t(data[3] << 8) + uint16_t(data[4]) + 50000.0f) / 100.0f;
      const float acceleration_x = (int16_t(data[5] << 8) + int16_t(data[6])) / 1000.0f;
      const float acceleration_y = (int16_t(data[7] << 8) + int16_t(data[8])) / 1000.0f;
      const float acceleration_z = (int16_t(data[9] << 8) + int16_t(data[10])) / 1000.0f;
      const float battery_voltage = (uint16_t(data[11] << 8) + uint16_t(data[12])) / 1000.0f;

      result.humidity = humidity;
      result.temperature = temperature;
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

  ESP_LOGD(TAG, "Got RuuviTag (%s):", device.address_str().c_str());

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
