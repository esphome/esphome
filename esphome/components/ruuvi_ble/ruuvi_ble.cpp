#include "ruuvi_ble.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ruuvi_ble {

static const char *const TAG = "ruuvi_ble";

bool parse_ruuvi_data_byte(const esp32_ble_tracker::adv_data_t &adv_data, RuuviParseResult &result) {
  const uint8_t data_type = adv_data[0];
  const auto *data = &adv_data[1];
  switch (data_type) {
    case 0x03: {  // RAWv1
      if (adv_data.size() != 14)
        return false;

      const uint8_t temp_sign = (data[1] >> 7) & 1;
      const float temp_val = (data[1] & 0x7F) + (data[2] / 100.0f);
      const float temperature = temp_sign == 0 ? temp_val : -1 * temp_val;

      const float humidity = data[0] * 0.5f;
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
      result.acceleration =
          sqrtf(acceleration_x * acceleration_x + acceleration_y * acceleration_y + acceleration_z * acceleration_z);
      result.battery_voltage = battery_voltage;

      return true;
    }
    case 0x05: {  // RAWv2
      if (adv_data.size() != 24)
        return false;

      const float temperature = (int16_t(data[0] << 8) + int16_t(data[1])) * 0.005f;
      const float humidity = (uint16_t(data[2] << 8) | uint16_t(data[3])) / 400.0f;
      const float pressure = ((uint16_t(data[4] << 8) | uint16_t(data[5])) + 50000.0f) / 100.0f;
      const float acceleration_x = (int16_t(data[6] << 8) + int16_t(data[7])) / 1000.0f;
      const float acceleration_y = (int16_t(data[8] << 8) + int16_t(data[9])) / 1000.0f;
      const float acceleration_z = (int16_t(data[10] << 8) + int16_t(data[11])) / 1000.0f;

      const uint16_t power_info = (uint16_t(data[12] << 8) | data[13]);
      const float battery_voltage = ((power_info >> 5) + 1600.0f) / 1000.0f;
      const float tx_power = ((power_info & 0x1F) * 2.0f) - 40.0f;

      const float movement_counter = float(data[14]);
      const float measurement_sequence_number = float(uint16_t(data[15] << 8) | uint16_t(data[16]));

      result.temperature = data[0] == 0x7F && data[1] == 0xFF ? NAN : temperature;
      result.humidity = data[2] == 0xFF && data[3] == 0xFF ? NAN : humidity;
      result.pressure = data[4] == 0xFF && data[5] == 0xFF ? NAN : pressure;
      result.acceleration_x = data[6] == 0xFF && data[7] == 0xFF ? NAN : acceleration_x;
      result.acceleration_y = data[8] == 0xFF && data[9] == 0xFF ? NAN : acceleration_y;
      result.acceleration_z = data[10] == 0xFF && data[11] == 0xFF ? NAN : acceleration_z;
      result.acceleration = result.acceleration_x == NAN || result.acceleration_y == NAN || result.acceleration_z == NAN
                                ? NAN
                                : sqrtf(acceleration_x * acceleration_x + acceleration_y * acceleration_y +
                                        acceleration_z * acceleration_z);
      result.battery_voltage = (power_info >> 5) == 0x7FF ? NAN : battery_voltage;
      result.tx_power = (power_info & 0x1F) == 0x1F ? NAN : tx_power;
      result.movement_counter = movement_counter;
      result.measurement_sequence_number = measurement_sequence_number;

      return true;
    }
    default:
      return false;
  }
}
optional<RuuviParseResult> parse_ruuvi(const esp32_ble_tracker::ESPBTDevice &device) {
  bool success = false;
  RuuviParseResult result{};
  for (auto &it : device.get_manufacturer_datas()) {
    bool is_ruuvi = it.uuid.contains(0x99, 0x04);
    if (!is_ruuvi)
      continue;

    if (parse_ruuvi_data_byte(it.data, result))
      success = true;
  }
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
    ESP_LOGD(TAG, "  Humidity: %.2f%%", *res->humidity);
  }
  if (res->temperature.has_value()) {
    ESP_LOGD(TAG, "  Temperature: %.2f°C", *res->temperature);
  }
  if (res->pressure.has_value()) {
    ESP_LOGD(TAG, "  Pressure: %.2fhPa", *res->pressure);
  }
  if (res->acceleration.has_value()) {
    ESP_LOGD(TAG, "  Acceleration: %.3fG", *res->acceleration);
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
  if (res->tx_power.has_value()) {
    ESP_LOGD(TAG, "  TX Power: %.0fdBm", *res->tx_power);
  }
  if (res->movement_counter.has_value()) {
    ESP_LOGD(TAG, "  Movement Counter: %.0f", *res->movement_counter);
  }
  if (res->measurement_sequence_number.has_value()) {
    ESP_LOGD(TAG, "  Measurement Sequence Number: %.0f", *res->measurement_sequence_number);
  }

  return true;
}

}  // namespace ruuvi_ble
}  // namespace esphome

#endif
